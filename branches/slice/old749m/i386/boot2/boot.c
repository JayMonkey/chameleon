/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *          INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *  This software is supplied under the terms of a license  agreement or 
 *  nondisclosure agreement with Intel Corporation and may not be copied 
 *  nor disclosed except in accordance with the terms of that agreement.
 *
 *  Copyright 1988, 1989 by Intel Corporation
 */

/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */

/*
 * Completely reworked by Sam Streeper (sam_s@NeXT.com)
 * Reworked again by Curtis Galloway (galloway@NeXT.com)
 */


#include "boot.h"
#include "bootstruct.h"
#include "fake_efi.h"
#include "acpi_patcher.h"
#include "sl.h"
#include "libsa.h"
#include "ramdisk.h"
#include "platform.h"
#include "graphics.h"
#include "pci.h"

#include "modules.h"

#define DEBUG 0

long gBootMode; /* defaults to 0 == kBootModeNormal */
bool gOverrideKernel;
static char gBootKernelCacheFile[512];
static char gCacheNameAdler[64 + 256];
char *gPlatformName = gCacheNameAdler;
char gRootDevice[512];
char gMKextName[512];
char gMacOSVersion[8];
void *gRootPCIDev;
void *gPlatform;
void *gBootOrder;
void *gSMBIOS;
int gDualLink;
#ifndef OPTION_ROM
bool gEnableCDROMRescan;
bool gScanSingleDrive;
#endif
config_file_t    systemVersion;		// system.plist of booting partition


int     bvCount = 0;
//int	menucount = 0;
int     gDeviceCount = 0; 

BVRef   bvr;
BVRef   menuBVR;
BVRef   bvChain;

//static void selectBiosDevice(void);
//static unsigned long Adler32(unsigned char *buffer, long length);
static bool getOSVersion(char *str);

#ifndef OPTION_ROM
static bool gUnloadPXEOnExit = false;
#endif

/*
 * How long to wait (in seconds) to load the
 * kernel after displaying the "boot:" prompt.
 */
#define kBootErrorTimeout 5

/*
 * Default path to kernel cache file
 */
 //Slice - first one for Leopard
#define kDefaultCachePath "/System/Library/Caches/com.apple.kernelcaches/"
#define kDefaultCachePathSnow "/System/Library/Caches/com.apple.kext.caches/Startup/"

//==========================================================================
// Zero the BSS.

static void zeroBSS(void)
{
	extern char _DATA__bss__begin, _DATA__bss__end;
	extern char _DATA__common__begin, _DATA__common__end;
	
	bzero(&_DATA__bss__begin, (&_DATA__bss__end - &_DATA__bss__begin));
	bzero(&_DATA__common__begin, (&_DATA__common__end - &_DATA__common__begin));
}

//==========================================================================
// Malloc error function

static void malloc_error(char *addr, size_t size, const char *file, int line)
{
	stop("\nMemory allocation error! Addr=0x%x, Size=0x%x, File=%s, Line=%d\n", (unsigned)addr, (unsigned)size, file, line);
}

//==========================================================================
//Initializes the runtime.  Right now this means zeroing the BSS and initializing malloc.
//
void initialize_runtime(void)
{
	zeroBSS();
	malloc_init(0, 0, 0, malloc_error);
}

//==========================================================================
// execKernel - Load the kernel image (mach-o) and jump to its entry point.

int ExecKernel(void *binary)
{
    entry_t                   kernelEntry;
    int                       ret;
	
    bootArgs->kaddr = bootArgs->ksize = 0;
	execute_hook("ExecKernel", (void*)binary, NULL, NULL, NULL);

    ret = DecodeKernel(binary,
                       &kernelEntry,
                       (char **) &bootArgs->kaddr,
                       (int *)&bootArgs->ksize );

    if ( ret != 0 )
        return ret;
    // Reserve space for boot args
    reserveKernBootStruct();
	
    // Load boot drivers from the specifed root path.
	execute_hook("DecodedKernel", (void*)binary, NULL, NULL, NULL);

	
	setupFakeEfi();
	
    if (!gHaveKernelCache) {
		LoadDrivers("/");
    }
	
    clearActivityIndicator();
	
    if (gErrors) {
        msglog("Errors encountered while starting up the computer.\n");
       // printf("Pausing %d seconds...\n", kBootErrorTimeout);
       // sleep(kBootErrorTimeout);
    }
	
	md0Ramdisk();
	
    verbose("Starting Darwin %s\n",( archCpuType == CPU_TYPE_I386 ) ? "i386" : "x86_64");
#ifndef OPTION_ROM
    // Cleanup the PXE base code.
	
    if ( (gBootFileType == kNetworkDeviceType) && gUnloadPXEOnExit ) {
		if ( (ret = nbpUnloadBaseCode()) != nbpStatusSuccess )
        {
        	verbose("nbpUnloadBaseCode error %d\n", (int) ret);
            sleep(2);
        }
    }
#endif 
    bool dummyVal;
	if (getBoolForKey(kWaitForKeypressKey, &dummyVal, &bootInfo->bootConfig) && dummyVal)
	{
		printf("Press any key to continue...");
		getc();
	}
		
	execute_hook("Kernel Start", (void*)kernelEntry, (void*)bootArgs, NULL, NULL);	// Notify modules that the kernel is about to be started

	if (bootArgs->Video.v_display == VGA_TEXT_MODE || gVerboseMode)
	{
//		setVideoMode( GRAPHICS_MODE, 0 );
		// Draw gray screen. NOTE: no boot image, that's in the gui module
#ifndef OPTION_ROM
		if(!gVerboseMode) drawColorRectangle(0, 0, DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, 0x01); //Slice -???
#endif
		setVideoMode( GRAPHICS_MODE, 0 );
	}

	
	setupBooterLog();
	
    finalizeBootStruct();
    	
	// Jump to kernel's entry point. There's no going back now.
    startprog( kernelEntry, bootArgs );
	
    // Not reached
	
    return 0;
}

//==========================================================================
// This is the entrypoint from real-mode which functions exactly as it did
// before. Multiboot does its own runtime initialization, does some of its
// own things, and then calls common_boot.
void boot(int biosdev)
{
	initialize_runtime();
	// Enable A20 gate before accessing memory above 1Mb.
	enableA20();
	common_boot(biosdev);
}

//==========================================================================
// The 'main' function for the booter. Called by boot0 when booting
// from a block device, or by the network booter.
//
// arguments:
//   biosdev - Value passed from boot1/NBP to specify the device
//             that the booter was loaded from.
//
// If biosdev is kBIOSDevNetwork, then this function will return if
// booting was unsuccessful. This allows the PXE firmware to try the
// next boot device on its list.
void common_boot(int biosdev)
{
    int      status;
	int		trycache;
    char     *bootFile;
    unsigned long adler32;
    bool     quiet;
    bool     firstRun = true;
    bool     instantMenu;
#ifndef OPTION_ROM
    bool     rescanPrompt;
#endif
    unsigned int allowBVFlags = kBVFlagSystemVolume|kBVFlagForeignBoot;
    unsigned int denyBVFlags = kBVFlagEFISystem;
	
#ifndef OPTION_ROM
    // Set reminder to unload the PXE base code. Neglect to unload
    // the base code will result in a hang or kernel panic.
    gUnloadPXEOnExit = true;
#endif
	
    // Record the device that the booter was loaded from.
    gBIOSDev = biosdev & kBIOSDevMask;  // 0xff
	
    // Initialize boot info structure.
    initKernBootStruct();
	
	initBooterLog();
	
    // Setup VGA text mode.
    // Not sure if it is safe to call setVideoMode() before the
    // config table has been loaded. Call video_mode() instead.
    video_mode( 2 );  // 80x25 mono text mode.
	
    // Scan and record the system's hardware information.
    scan_platform();

    // First get info for boot volume.
    scanBootVolumes(gBIOSDev, 0);
    bvChain = getBVChainForBIOSDev(gBIOSDev);
	
#if DEBUG	
    printf("get bvChain dev=%02x type=%02x part_no=%d\n", bvChain->biosdev, bvChain->type, bvChain->part_no); //dev=0x80 - flash-stick
//		getc();
#endif
	
    setBootGlobals(bvChain);
	
    // Load boot.plist config file
    status = loadSystemConfig(&bootInfo->bootConfig);

    if (getBoolForKey(kQuietBootKey, &quiet, &bootInfo->bootConfig) && quiet) {
        gBootMode |= kBootModeQuiet;
    }
	
    // Override firstRun to get to the boot menu instantly by setting "Instant Menu"=y in system config
    if (getBoolForKey(kInsantMenuKey, &instantMenu, &bootInfo->bootConfig) && instantMenu) {
        firstRun = false;
    }
	
#ifndef OPTION_ROM
	// Enable touching a single BIOS device only if "Scan Single Drive"=y is set in system config.
    if (getBoolForKey(kScanSingleDriveKey, &gScanSingleDrive, &bootInfo->bootConfig) && gScanSingleDrive) {
        gScanSingleDrive = true;
    }
	
	// Create a list of partitions on device(s).
    if (gScanSingleDrive)
	{
		scanBootVolumes(gBIOSDev, &bvCount);
    } 
	else 
#endif
	{
		scanDisks(gBIOSDev, &bvCount);
    }
	
	// Create a separated bvr chain using the specified filters.
    bvChain = newFilteredBVChain(0x80, 0xFF, allowBVFlags, denyBVFlags, &gDeviceCount);
	gBootVolume = selectBootVolume(bvChain);
	
#if DEBUG	
    printf("separated bvChain dev=%02x type=%02x part_no=%d\n", bvChain->biosdev, bvChain->type, bvChain->part_no); //dev=0x81 - HDD
	pause();
#endif
	
	// Intialize module system
	if(init_module_system())
	{
#if DEBUG	
		printf("begin load_all_modules\n");
//		pause();
#endif
		
		load_all_modules();
	}
	
	execute_hook("ModulesLoaded", NULL, NULL, NULL, NULL);
#if DEBUG
    printf("ModulesLoaded\n");
//	pause();
#endif
	
	
#ifndef OPTION_ROM
    // Loading preboot ramdisk if exists.
    loadPrebootRAMDisk();
	
    // Disable rescan option by default
    gEnableCDROMRescan = false;
	
    // Enable it with Rescan=y in system config
    if (getBoolForKey(kRescanKey, &gEnableCDROMRescan, &bootInfo->bootConfig) && gEnableCDROMRescan) {
        gEnableCDROMRescan = true;
    }
	
    // Ask the user for Rescan option by setting "Rescan Prompt"=y in system config.
    rescanPrompt = false;
    if (getBoolForKey(kRescanPromptKey, &rescanPrompt , &bootInfo->bootConfig) && rescanPrompt && biosDevIsCDROM(gBIOSDev)) {
        gEnableCDROMRescan = promptForRescanOption();
    }
#endif
	
#if DEBUG
    printf(" Default: %x, ->gBootVolume: %x, ->part_no: %d ->flags: %x\n", gBootVolume, gBootVolume->biosdev, gBootVolume->part_no, gBootVolume->flags);
    printf(" bt(0,0): %x, ->gBIOSBootVolume: %x, ->part_no: %d ->flags: %x\n", gBIOSBootVolume, gBIOSBootVolume->biosdev, gBIOSBootVolume->part_no, gBIOSBootVolume->flags);
    pause();
	/* Results
	 Rescan found 1 HDD and bvCount=4
	 bvr: 836bc10, dev: 80, part: 4, flags: 4a, vis: 1
	 bvr: 836bce0, dev: 80, part: 3, flags: 4b, vis: 1
	 bvr: 836be90, dev: 80, part: 2, flags: 4, vis: 1
	 bvr: 836bf60, dev: 80, part: 1, flags: a, vis: 0
	 count: 3
	 
	 foundPrimary and flags bvr1=a bvr2=4b
	 Default: 836bce0, ->biosdev: 80, ->part_no: 3 ->flags: 4b //booted partition
	 bt(0,0): 8262030, ->biosdev: 80, ->part_no: 3 ->flags: 4b //unknown pointer
	
	 */
#endif
	
	
	
    setBootGlobals(bvChain);
	
    // Parse args, load and start kernel.
    while (1) {
        const char *val;
        int len;
   //     int trycache;
        long flags, cachetime, kerneltime, exttime, sleeptime, time;
        int ret = -1;
        void *binary = (void *)kLoadAddr;
        bool tryresume;
        bool tryresumedefault;
        bool forceresume;
		
        // additional variable for testing alternate kernel image locations on boot helper partitions.
        char     bootFileSpec[512];
		
        // Initialize globals.
		
        sysConfigValid = false;
        gErrors        = false;
        trycache		=	1;
		
        status = getBootOptions(firstRun);
        firstRun = false;
        if (status == -1) continue;
		
        status = processBootOptions();
        // Status==1 means to chainboot
        if ( status ==  1 ) break;
        // Status==-1 means that the config file couldn't be loaded or that gBootVolume is NULL
        if ( status == -1 )
        {
			// gBootVolume == NULL usually means the user hit escape.
			if(gBootVolume == NULL)
			{
				freeFilteredBVChain(bvChain);
#ifndef OPTION_ROM
				if (gEnableCDROMRescan)
					rescanBIOSDevice(gBIOSDev);
#endif
				
				bvChain = newFilteredBVChain(0x80, 0xFF, allowBVFlags, denyBVFlags, &gDeviceCount);
				setBootGlobals(bvChain);
			}
#if DEBUG
			verbose("After rescan\n");
			verbose(" System: %x, ->biosdev: %x, ->part_no: %d ->flags: %x\n", gBootVolume, gBootVolume->biosdev, gBootVolume->part_no, gBootVolume->flags);
			verbose(" Booter: %x, ->biosdev: %x, ->part_no: %d ->flags: %x\n", gBIOSBootVolume, gBIOSBootVolume->biosdev, gBIOSBootVolume->part_no, gBIOSBootVolume->flags);
			// getc();
#endif
			
			continue;
        }
		
		
		// Find out which version mac os we're booting.
		getOSVersion(gMacOSVersion);
		
		if (platformCPUFeature(CPU_FEATURE_EM64T)) {
			archCpuType = CPU_TYPE_X86_64;
		} else {
			archCpuType = CPU_TYPE_I386;
		}
		if (getValueForKey(karch, &val, &len, &bootInfo->bootConfig)) {
			if (strncmp(val, "i386", 4) == 0) {
				archCpuType = CPU_TYPE_I386;
			}
		}
		
		// Notify modules that we are attempting to boot
		execute_hook("PreBoot", NULL, NULL, NULL, NULL);

		if (!getBoolForKey (kWake, &tryresume, &bootInfo->bootConfig)) {
			tryresume = true;
			tryresumedefault = true;
		} else {
			tryresumedefault = false;
		}
		
		if (!getBoolForKey (kForceWake, &forceresume, &bootInfo->bootConfig)) {
			forceresume = false;
		}
		
		if (forceresume) {
			tryresume = true;
			tryresumedefault = false;
		}
		
		while (tryresume) {
			const char *tmp;
			BVRef bvr;
			if (!getValueForKey(kWakeImage, &val, &len, &bootInfo->bootConfig))
				val="/private/var/vm/sleepimage";
			
			// Do this first to be sure that root volume is mounted
			ret = GetFileInfo(0, val, &flags, &sleeptime);
			
			if ((bvr = getBootVolumeRef(val, &tmp)) == NULL)
				break;
			
			// Can't check if it was hibernation Wake=y is required
			if (bvr->modTime == 0 && tryresumedefault)
				break;
			
			if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat))
				break;
			
			if (!forceresume && ((sleeptime+3)<bvr->modTime)) {
				printf ("Hibernate image is too old by %d seconds. Use ForceWake=y to override\n",bvr->modTime-sleeptime);
				break;
			}
			
			HibernateBoot((char *)val);
			break;
		}
		
        // Reset cache name.
        bzero(gCacheNameAdler + 64, sizeof(gCacheNameAdler) - 64);
		
        sprintf(gCacheNameAdler + 64, "%s,%s", gRootDevice, bootInfo->bootFile);
		
        adler32 = Adler32((unsigned char *)gCacheNameAdler, sizeof(gCacheNameAdler));
		
        if (getValueForKey(kKernelCacheKey, &val, &len, &bootInfo->bootConfig) == true) {
            strlcpy(gBootKernelCacheFile, val, len+1);
        } else {
			if(gMacOSVersion[3] >= '6') {
				sprintf(gBootKernelCacheFile, "kernelcache_%s", /*kDefaultCachePathSnow,*/ (archCpuType == CPU_TYPE_I386) ? "i386" : "x86_64"); //, adler32);
				msglog("search for kernelcache %s\n", gBootKernelCacheFile);
				int lnam = sizeof(gBootKernelCacheFile) + 9; //with adler32
				//Slice - TODO
				/*
				 - but the name is longer .adler32 and more...
				 kernelcache_i386.E102928C.qSs0
				 so will opendir and scan for some files
				*/ 
				char* name;
				long flagsC;
				long timeC;
				struct dirstuff* cacheDir = opendir(kDefaultCachePathSnow);
				while(readdir(cacheDir, (const char**)&name, &flagsC, &timeC) >= 0)
				{
					if(strstr(name, gBootKernelCacheFile)) //?
					{
						if(name[lnam] == '.') continue;
						//char* tmp = malloc(strlen(name) + 1);
						sprintf(gBootKernelCacheFile, "%s%s", kDefaultCachePathSnow, name);
						verbose("find kernelcache=%s\n", gBootKernelCacheFile);
						break;
					}
				}
		//		close(cacheDir);
						
			}
			else //if(gMacOSVersion[3] == '5')
				sprintf(gBootKernelCacheFile, "%skernelcache", kDefaultCachePath);
        }
		
        // Check for cache file.
        trycache = (((gBootMode & kBootModeSafe) == 0) &&
                    !gOverrideKernel &&
                    (gBootFileType == kBlockDeviceType) &&
                    (gMKextName[0] == '\0') &&
                    (gBootKernelCacheFile[0] != '\0')); // &&
//					(gMacOSVersion[3] != '4'));
		verbose("Try cache %s %s\n", gBootKernelCacheFile, trycache?"YES":"NO");
		
		verbose("Loading Darwin %s\n", gMacOSVersion);
		
        if (trycache) do {
			
            // if we haven't found the kernel yet, don't use the cache
            ret = GetFileInfo(NULL, bootInfo->bootFile, &flags, &kerneltime);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat)) {
                trycache = 0;
                break;
            }
            ret = GetFileInfo(NULL, gBootKernelCacheFile, &flags, &cachetime);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat)
                || (cachetime < kerneltime)) {
                trycache = 0;
                verbose("Try cache failed\n");
                break;
            }
            ret = GetFileInfo("/System/Library/", "Extensions", &flags, &exttime);
            if ((ret == 0) && ((flags & kFileTypeMask) == kFileTypeDirectory)
                && (cachetime < exttime)) {
                trycache = 0;
                break;
            }
            if (kerneltime > exttime) {
                exttime = kerneltime;
            }
            if (cachetime != (exttime + 1)) {
                trycache = 0;
                break;
            }
        } while (0);
		
        do {
            if (trycache) {
                bootFile = gBootKernelCacheFile;
                verbose("Loading kernel cache %s\n", bootFile);
                ret = LoadFile(bootFile);
                binary = (void *)kLoadAddr;
                if (ret >= 0) {
                    break;
                }
            }
            bootFile = bootInfo->bootFile;
			
            // Try to load kernel image from alternate locations on boot helper partitions.
            sprintf(bootFileSpec, "com.apple.boot.P/%s", bootFile);
            ret = GetFileInfo(NULL, bootFileSpec, &flags, &time); 
  	  	    if (ret == -1)
  	  	    {
				sprintf(bootFileSpec, "com.apple.boot.R/%s", bootFile);
				ret = GetFileInfo(NULL, bootFileSpec, &flags, &time); 
				if (ret == -1)
				{
					sprintf(bootFileSpec, "com.apple.boot.S/%s", bootFile);
					ret = GetFileInfo(NULL, bootFileSpec, &flags, &time); 
					if (ret == -1)
					{
						// Not found any alternate locations, using the original kernel image path.
						strcpy(bootFileSpec, bootFile);
					}
				}
            }
			
            verbose("Loading kernel %s\n", bootFileSpec);
            ret = LoadThinFatFile(bootFileSpec, &binary);
            if (ret <= 0 && archCpuType == CPU_TYPE_X86_64)
            {
				archCpuType = CPU_TYPE_I386;
				ret = LoadThinFatFile(bootFileSpec, &binary);				
            }
			
        } while (0);
		
        clearActivityIndicator();
#if DEBUG
        printf("Pausing...");
        sleep(8);
#endif
		
        if (ret <= 0) {
			printf("Can't find %s\n", bootFile);
			
			sleep(1);
#ifndef OPTION_ROM
            if (gBootFileType == kNetworkDeviceType) {
                // Return control back to PXE. Don't unload PXE base code.
                gUnloadPXEOnExit = false;
                break;
            }
#endif
        } else {
            /* Won't return if successful. */
			// Notify modules that ExecKernel is about to be called
            ret = ExecKernel(binary);
        }

    } /* while(1) */
    
    // chainboot
    if (status==1) {
		if (getVideoMode() == GRAPHICS_MODE) {	// if we are already in graphics-mode,
			setVideoMode(VGA_TEXT_MODE, 0);	// switch back to text mode
		}
    }
#ifndef OPTION_ROM
    if ((gBootFileType == kNetworkDeviceType) && gUnloadPXEOnExit) {
		nbpUnloadBaseCode();
    }
#endif
}

/*!
 Selects a new BIOS device, taking care to update the global state appropriately.
 */
/*
 static void selectBiosDevice(void)
 {
 struct DiskBVMap *oldMap = diskResetBootVolumes(gBIOSDev);
 CacheReset();
 diskFreeMap(oldMap);
 oldMap = NULL;
 
 int dev = selectAlternateBootDevice(gBIOSDev);
 
 BVRef bvchain = scanBootVolumes(dev, 0);
 BVRef bootVol = selectBootVolume(bvchain);
 gBootVolume = bootVol;
 setRootVolume(bootVol);
 gBIOSDev = dev;
 }
 */

static bool getOSVersion(char *str)
{
	bool valid = false;
//	config_file_t systemVersion;
	const char *val;
	int len;
//	msglog("Address loadConfigFile=%x  bootInfo=%x\n", &loadConfigFile, &bootInfo);
	if (!loadConfigFile("/System/Library/CoreServices/SystemVersion.plist", &systemVersion))
	{
		valid = true;
	}
	else if (!loadConfigFile("/System/Library/CoreServices/ServerVersion.plist", &systemVersion))
	{
		valid = true;
	}
	
	if (valid)
	{
		if  (getValueForKey(kProductVersion, &val, &len, &systemVersion))
		{
			// getValueForKey uses const char for val
			// so copy it and trim
			*str = '\0';
			strncat(str, val, MIN(len, 4));
		}
		else
			valid = false;
	}
	
	return valid;
}

#define BASE 65521L /* largest prime smaller than 65536 */
#define NMAX 5000
// NMAX (was 5521) the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1

#define DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);
/*
unsigned long Adler32(unsigned char *buf, long len)
{
    unsigned long s1 = 1; // adler & 0xffff;
    unsigned long s2 = 0; // (adler >> 16) & 0xffff;
    unsigned long result;
    int k;
	
    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            DO16(buf);
            buf += 16;
            k -= 16;
        }
        if (k != 0) do {
            s1 += *buf++;
            s2 += s1;
        } while (--k);
        s1 %= BASE;
        s2 %= BASE;
    }
    result = (s2 << 16) | s1;
    return OSSwapHostToBigInt32(result);
}
*/