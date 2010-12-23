/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  drivers.c - Driver Loading Functions.
 *
 *  Copyright (c) 2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <mach-o/fat.h>
#include <libkern/OSByteOrder.h>
#include <mach/machine.h>

#include "sl.h"
#include "boot.h"
#include "bootstruct.h"
#include "xml.h"
#include "ramdisk.h"
#include "drivers.h"
#include "modules.h"

//extern char gMacOSVersion; //Slice - defined in boot.h

#ifndef OPTION_ROM
long (*LoadExtraDrivers_p)(FileLoadDrivers_t FileLoadDrivers_p);
#endif

//extern unsigned long Adler32( unsigned char * buffer, long length );

long FileLoadDrivers(char *dirSpec, long plugin);
#ifndef OPTION_ROM
long NetLoadDrivers(char *dirSpec);
#endif
long LoadDriverMKext(char *fileSpec);
long LoadDriverPList(char *dirSpec, char *name, long bundleType);
long LoadMatchedModules(void);
long MatchPersonalities(void);
long MatchLibraries(void);
#ifdef NOTDEF
static ModulePtr FindModule(char *name);
static void ThinFatFile(void **loadAddrP, unsigned long *lengthP);
#endif
static long ParseXML(char *buffer, ModulePtr *module, TagPtr *personalities);
long InitDriverSupport(void);

static ModulePtr gModuleHead, gModuleTail;
static TagPtr    gPersonalityHead, gPersonalityTail;
char *    gExtensionsSpec;
char *    gDriverSpec;
char *    gFileSpec;
char *    gTempSpec;
char *    gFileName;

unsigned long
Adler32( unsigned char * buffer, long length )
{
    long          cnt;
    unsigned long result, lowHalf, highHalf;
    
    lowHalf  = 1;
    highHalf = 0;
	
	for ( cnt = 0; cnt < length; cnt++ )
    {
        if ((cnt % 5000) == 0)
        {
            lowHalf  %= 65521L;
            highHalf %= 65521L;
        }
		
        lowHalf  += buffer[cnt];
        highHalf += lowHalf;
    }
	
	lowHalf  %= 65521L;
	highHalf %= 65521L;
	
	result = (highHalf << 16) | lowHalf;
	
	return result;
}


//==========================================================================
// InitDriverSupport

long
InitDriverSupport( void )
{
    gExtensionsSpec = malloc( 4096 );
    gDriverSpec     = malloc( 4096 );
    gFileSpec       = malloc( 4096 );
    gTempSpec       = malloc( 4096 );
    gFileName       = malloc( 4096 );
	
    if ( !gExtensionsSpec || !gDriverSpec || !gFileSpec || !gTempSpec || !gFileName )
        stop("InitDriverSupport error");
	
    return 0;
}

//==========================================================================
// LoadDrivers

long LoadDrivers( char * dirSpec )
{	
	const char * sLoadExtra = "LoadDrivers: Loading Extra drivers %s...\n";
    char dirSpecExtra[1024];
	
    if ( InitDriverSupport() != 0 )
        return 0;
	
#ifndef OPTION_ROM
    // Load extra drivers if a hook has been installed.
    if (LoadExtraDrivers_p != NULL)
    {
        (*LoadExtraDrivers_p)(&FileLoadDrivers);
    }
	
    if ( gBootFileType == kNetworkDeviceType )
    {
        if (NetLoadDrivers(dirSpec) != 0) {
            error("Could not load drivers from the network\n");
            return -1;
        }
    }
    else
#endif
		if ( gBootFileType == kBlockDeviceType )
		{
			// First try to load Extra extensions from the ramdisk if isn't aliased as bt(0,0).
#ifndef OPTION_ROM
			// First try a specfic OS version folder ie 10.5
			sprintf(dirSpecExtra, "rd(0,0)/Extra/%s/", &gMacOSVersion);
			if (FileLoadDrivers(dirSpecExtra, 0) != 0)
			{	
				// Next we'll try the base
				strcpy(dirSpecExtra, "rd(0,0)/Extra/");
				FileLoadDrivers(dirSpecExtra, 0);
			}
#endif
			
			// First try a specfic OS version folder ie 10.5
			sprintf(dirSpecExtra, "/Extra/%s/", &gMacOSVersion);
			if (FileLoadDrivers(dirSpecExtra, 0) != 0)
			{	
				// Next try to load Extra extensions from the selected root partition.
				strcpy(dirSpecExtra, "/Extra/");
				if (FileLoadDrivers(dirSpecExtra, 0) != 0)
				{
					// If failed, then try to load Extra extensions from the boot partition
					// in case we have a separate booter partition or a bt(0,0) aliased ramdisk.
					if ( !(gBIOSBootVolume->biosdev == gBootVolume->biosdev  && gBIOSBootVolume->part_no == gBootVolume->part_no)
#ifndef OPTION_ROM
						|| (gRAMDiskVolume && gRAMDiskBTAliased) )
#else
						)
#endif
					{
						// First try a specfic OS version folder ie 10.5
						sprintf(dirSpecExtra, "bt(0,0)/Extra/%s/", &gMacOSVersion);
						if (FileLoadDrivers(dirSpecExtra, 0) != 0)
						{	
							// Next we'll try the base
							strcpy(dirSpecExtra, "bt(0,0)/Extra/");
							verbose(sLoadExtra, dirSpecExtra);
							FileLoadDrivers(dirSpecExtra, 0);
						}
					}
				}
				
			}			
			// TODO: fix this, the order does matter, and it's not correct now.
			// Also try to load Extensions from boot helper partitions.
			if (gBootVolume->flags & kBVFlagBooter)
			{
				strcpy(dirSpecExtra, "/com.apple.boot.P/System/Library/");
				if (FileLoadDrivers(dirSpecExtra, 0) != 0)
				{
					strcpy(dirSpecExtra, "/com.apple.boot.R/System/Library/");
					if (FileLoadDrivers(dirSpecExtra, 0) != 0)
					{
						strcpy(dirSpecExtra, "/com.apple.boot.S/System/Library/");
						FileLoadDrivers(dirSpecExtra, 0);
					}
				}
			}
			
			if (gMKextName[0] != '\0')
			{
				verbose("LoadDrivers: Loading from [%s]\n", gMKextName);
				if ( LoadDriverMKext(gMKextName) != 0 )
				{
					error("Could not load %s\n", gMKextName);
					return -1;
				}
			}
			else
			{
				strcpy(gExtensionsSpec, dirSpec);
				strcat(gExtensionsSpec, "System/Library/");
				FileLoadDrivers(gExtensionsSpec, 0);
			}
		}
		else
		{
			return 0;
		}
	
    MatchPersonalities();
	
    MatchLibraries();
	
    LoadMatchedModules();
	
    return 0;
}

//==========================================================================
// FileLoadMKext

static long
FileLoadMKext( const char * dirSpec, const char * extDirSpec )
{
	long  ret, flags, time, time2;
	char altDirSpec[512];
	
	sprintf (altDirSpec, "%s%s", dirSpec, extDirSpec);
	ret = GetFileInfo(altDirSpec, "Extensions.mkext", &flags, &time);
	if ((ret == 0) && ((flags & kFileTypeMask) == kFileTypeFlat))
	{
		ret = GetFileInfo(dirSpec, "Extensions", &flags, &time2);
		if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeDirectory) ||
			(((gBootMode & kBootModeSafe) == 0) && (time == (time2 + 1))))
		{
			sprintf(gDriverSpec, "%sExtensions.mkext", altDirSpec);
			verbose("LoadDrivers: Loading from [%s]\n", gDriverSpec);
			if (LoadDriverMKext(gDriverSpec) == 0) return 0;
		}
	}
	return -1;
}

//==========================================================================
// FileLoadDrivers

long
FileLoadDrivers( char * dirSpec, long plugin )
{
    long         ret, length, flags, time, bundleType;
    long long	 index;
    long         result = -1;
    const char * name;
	
    if ( !plugin )
    {
        // First try 10.6's path for loading Extensions.mkext.
        if (FileLoadMKext(dirSpec, "Caches/com.apple.kext.caches/Startup/") == 0)
			return 0;
		
        // Next try the legacy path.
        else if (FileLoadMKext(dirSpec, "") == 0)
			return 0;
		
        strcat(dirSpec, "Extensions");
    }
	
    index = 0;
    while (1) {
        ret = GetDirEntry(dirSpec, &index, &name, &flags, &time);
        if (ret == -1) break;
		
        // Make sure this is a directory.
        if ((flags & kFileTypeMask) != kFileTypeDirectory) continue;
        
        // Make sure this is a kext.
        length = strlen(name);
        if (strcmp(name + length - 5, ".kext")) continue;
		
        // Save the file name.
        strcpy(gFileName, name);
		
        // Determine the bundle type.
        sprintf(gTempSpec, "%s/%s", dirSpec, gFileName);
        ret = GetFileInfo(gTempSpec, "Contents", &flags, &time);
        if (ret == 0) bundleType = kCFBundleType2;
        else bundleType = kCFBundleType3;
		
        if (!plugin)
            sprintf(gDriverSpec, "%s/%s/%sPlugIns", dirSpec, gFileName,
                    (bundleType == kCFBundleType2) ? "Contents/" : "");
		
        ret = LoadDriverPList(dirSpec, gFileName, bundleType);
		
        if (result != 0)
			result = ret;
		
        if (!plugin) 
            FileLoadDrivers(gDriverSpec, 1); //Slice - "result =" as in RC3
    }
	
    return result;
}

//==========================================================================
// 
#ifndef OPTION_ROM
long
NetLoadDrivers( char * dirSpec )
{
    long tries;
	
#if NODEF
    long cnt;
	
    // Get the name of the kernel
    cnt = strlen(gBootFile);
    while (cnt--) {
        if ((gBootFile[cnt] == '\\')  || (gBootFile[cnt] == ',')) {
			cnt++;
			break;
        }
    }
#endif
	
    // INTEL modification
    sprintf(gDriverSpec, "%s%s.mkext", dirSpec, bootInfo->bootFile);
    
    verbose("NetLoadDrivers: Loading from [%s]\n", gDriverSpec);
    
    tries = 3;
    while (tries--)
    {
        if (LoadDriverMKext(gDriverSpec) == 0) break;
    }
    if (tries == -1) return -1;
	
    return 0;
}
#endif
//==========================================================================
// loadDriverMKext

long
LoadDriverMKext( char * fileSpec )
{
    unsigned long    driversAddr, driversLength;
    long             length;
    char             segName[32];
    DriversPackage * package;
	
#define GetPackageElement(e)     OSSwapBigToHostInt32(package->e)
	
    // Load the MKext.
    length = LoadThinFatFile(fileSpec, (void **)&package);
    if (length < sizeof (DriversPackage)) return -1;
	
	// call hook to notify modules that the mkext has been loaded
	execute_hook("LoadDriverMKext", (void*)fileSpec, (void*)package, (void*) &length, NULL);
	
	
    // Verify the MKext.
    if (( GetPackageElement(signature1) != kDriverPackageSignature1) ||
        ( GetPackageElement(signature2) != kDriverPackageSignature2) ||
        ( GetPackageElement(length)      > kLoadSize )               ||
        ( GetPackageElement(alder32)    !=
		 Adler32((unsigned char *)&package->version, GetPackageElement(length) - 0x10) ) )
    {
        return -1;
    }
	
    // Make space for the MKext.
    driversLength = GetPackageElement(length);
    driversAddr   = AllocateKernelMemory(driversLength);
	
    // Copy the MKext.
    memcpy((void *)driversAddr, (void *)package, driversLength);
	
    // Add the MKext to the memory map.
    sprintf(segName, "DriversPackage-%lx", driversAddr);
    AllocateMemoryRange(segName, driversAddr, driversLength,
                        kBootDriverTypeMKEXT);
	
    return 0;
}

//==========================================================================
// LoadDriverPList

long
LoadDriverPList( char * dirSpec, char * name, long bundleType )
{
    long      length, executablePathLength, bundlePathLength;
    ModulePtr module;
    TagPtr    personalities;
    char *    buffer = 0;
    char *    tmpExecutablePath = 0;
    char *    tmpBundlePath = 0;
    long      ret = -1;
	
    do {
        // Save the driver path.
        
        sprintf(gFileSpec, "%s/%s/%s", dirSpec, name,
                (bundleType == kCFBundleType2) ? "Contents/MacOS/" : "");
        executablePathLength = strlen(gFileSpec) + 1;
		
        tmpExecutablePath = malloc(executablePathLength);
        if (tmpExecutablePath == 0) break;
		
        strcpy(tmpExecutablePath, gFileSpec);
		
        sprintf(gFileSpec, "%s/%s", dirSpec, name);
        bundlePathLength = strlen(gFileSpec) + 1;
		
        tmpBundlePath = malloc(bundlePathLength);
        if (tmpBundlePath == 0) break;
		
        strcpy(tmpBundlePath, gFileSpec);
		
        // Construct the file spec to the plist, then load it.
		
        sprintf(gFileSpec, "%s/%s/%sInfo.plist", dirSpec, name,
                (bundleType == kCFBundleType2) ? "Contents/" : "");
		
        length = LoadFile(gFileSpec);
        if (length == -1) break;
		
        length = length + 1;
        buffer = malloc(length);
        if (buffer == 0) break;
		
        strlcpy(buffer, (char *)kLoadAddr, length);
		
        // Parse the plist.
		
        ret = ParseXML(buffer, &module, &personalities);
        if (ret != 0) { break; }
		
        // Allocate memory for the driver path and the plist.
		
        module->executablePath = tmpExecutablePath;
        module->bundlePath = tmpBundlePath;
        module->bundlePathLength = bundlePathLength;
        module->plistAddr = malloc(length);
		
        if ((module->executablePath == 0) || (module->bundlePath == 0) || (module->plistAddr == 0))
            break;
		
        // Save the driver path in the module.
        //strcpy(module->driverPath, tmpDriverPath);
        tmpExecutablePath = 0;
        tmpBundlePath = 0;
		
        // Add the plist to the module.
		
        strlcpy(module->plistAddr, (char *)kLoadAddr, length);
        module->plistLength = length;
		
        // Add the module to the end of the module list.
        
        if (gModuleHead == 0)
            gModuleHead = module;
        else
            gModuleTail->nextModule = module;
        gModuleTail = module;
		
        // Add the persionalities to the personality list.
		
        if (personalities) personalities = personalities->tag;
        while (personalities != 0)
        {
            if (gPersonalityHead == 0)
                gPersonalityHead = personalities->tag;
            else
                gPersonalityTail->tagNext = personalities->tag;
            
            gPersonalityTail = personalities->tag;
            personalities = personalities->tagNext;
        }
        
        ret = 0;
    }
    while (0);
    
    if ( buffer )        free( buffer );
    if ( tmpExecutablePath ) free( tmpExecutablePath );
    if ( tmpBundlePath ) free( tmpBundlePath );
	
    return ret;
}


//==========================================================================
// LoadMatchedModules

long LoadMatchedModules( void )
{
    TagPtr        prop;
    ModulePtr     module;
    char          *fileName, segName[32];
    DriverInfoPtr driver;
    long          length, driverAddr, driverLength;
    void          *executableAddr = 0;
	
	
    module = gModuleHead;
	
    while (module != 0)
    {
        if (module->willLoad)
        {
            prop = XMLGetProperty(module->dict, kPropCFBundleExecutable);
			
            if (prop != 0)
            {
                fileName = prop->string;
                sprintf(gFileSpec, "%s%s", module->executablePath, fileName);
                length = LoadThinFatFile(gFileSpec, &executableAddr);
				if (length == 0)
				{
					length = LoadFile(gFileSpec);
					executableAddr = (void *)kLoadAddr;
				}
                //printf("%s length = %d addr = 0x%x\n", gFileSpec, length, driverModuleAddr); getc();
            }
            else
                length = 0;
			
            if (length != -1)
            {
				//driverModuleAddr = (void *)kLoadAddr;
                //if (length != 0)
                //{
				//    ThinFatFile(&driverModuleAddr, &length);
				//}
				
                // Make make in the image area.
				
				execute_hook("LoadMatchedModules", module, &length, executableAddr, NULL);
				
                driverLength = sizeof(DriverInfo) + module->plistLength + length + module->bundlePathLength;
                driverAddr = AllocateKernelMemory(driverLength);
				
                // Set up the DriverInfo.
                driver = (DriverInfoPtr)driverAddr;
                driver->plistAddr = (char *)(driverAddr + sizeof(DriverInfo));
                driver->plistLength = module->plistLength;
                if (length != 0)
                {
                    driver->executableAddr = (void *)(driverAddr + sizeof(DriverInfo) +
													  module->plistLength);
                    driver->executableLength = length;
                }
                else
                {
                    driver->executableAddr   = 0;
                    driver->executableLength = 0;
                }
                driver->bundlePathAddr = (void *)(driverAddr + sizeof(DriverInfo) +
												  module->plistLength + driver->executableLength);
                driver->bundlePathLength = module->bundlePathLength;
				
                // Save the plist, module and bundle.
                strcpy(driver->plistAddr, module->plistAddr);
                if (length != 0)
                {
                    memcpy(driver->executableAddr, executableAddr, length);
                }
                strcpy(driver->bundlePathAddr, module->bundlePath);
				
                // Add an entry to the memory map.
                sprintf(segName, "Driver-%lx", (unsigned long)driver);
                AllocateMemoryRange(segName, driverAddr, driverLength,
                                    kBootDriverTypeKEXT);
            }
        }
        module = module->nextModule;
    }
	
    return 0;
}

//==========================================================================
// MatchPersonalities

long MatchPersonalities( void )
{
    /* IONameMatch support not implemented */
    return 0;
}

//==========================================================================
// MatchLibraries

long MatchLibraries( void )
{
    TagPtr     prop, prop2;
    ModulePtr  module, module2;
    long       done;
	
    do {
        done = 1;
        module = gModuleHead;
        
        while (module != 0)
        {
            if (module->willLoad == 1)
            {
                prop = XMLGetProperty(module->dict, kPropOSBundleLibraries);
                if (prop != 0)
                {
                    prop = prop->tag;
                    while (prop != 0)
                    {
                        module2 = gModuleHead;
                        while (module2 != 0)
                        {
                            prop2 = XMLGetProperty(module2->dict, kPropCFBundleIdentifier);
                            if ((prop2 != 0) && (!strcmp(prop->string, prop2->string)))
                            {
                                if (module2->willLoad == 0) module2->willLoad = 1;
                                break;
                            }
                            module2 = module2->nextModule;
                        }
                        prop = prop->tagNext;
                    }
                }
                module->willLoad = 2;
                done = 0;
            }
            module = module->nextModule;
        }
    }
    while (!done);
	
    return 0;
}


//==========================================================================
// FindModule

#if NOTDEF
static ModulePtr
FindModule( char * name )
{
    ModulePtr module;
    TagPtr    prop;
    
    module = gModuleHead;
    
    while (module != 0)
    {
        prop = GetProperty(module->dict, kPropCFBundleIdentifier);
        if ((prop != 0) && !strcmp(name, prop->string)) break;
        module = module->nextModule;
    }
    
    return module;
}
#endif /* NOTDEF */

//==========================================================================
// ParseXML

static long
ParseXML( char * buffer, ModulePtr * module, TagPtr * personalities )
{
	long       length, pos;
	TagPtr     moduleDict, required;
	ModulePtr  tmpModule;
	
    pos = 0;
	
    while (1)
    {
        length = XMLParseNextTag(buffer + pos, &moduleDict);
        if (length == -1) break;
		
        pos += length;
		
        if (moduleDict == 0) continue;
        if (moduleDict->type == kTagTypeDict) break;
		
        XMLFreeTag(moduleDict);
    }
	
    if (length == -1) return -1;
	
    required = XMLGetProperty(moduleDict, kPropOSBundleRequired);
    if ( (required == 0) ||
		(required->type != kTagTypeString) ||
		!strcmp(required->string, "Safe Boot"))
    {
        XMLFreeTag(moduleDict);
        return -2;
    }
	
    tmpModule = (ModulePtr)malloc(sizeof(Module));
    if (tmpModule == 0)
    {
        XMLFreeTag(moduleDict);
        return -1;
    }
    tmpModule->dict = moduleDict;
	
    // For now, load any module that has OSBundleRequired != "Safe Boot".
	
    tmpModule->willLoad = 1;
	
    *module = tmpModule;
	
    // Get the personalities.
	
    *personalities = XMLGetProperty(moduleDict, kPropIOKitPersonalities);
	
    return 0;
}

#if NOTDEF
static char gPlatformName[64];
#endif

long 
DecodeKernel(void *binary, entry_t *rentry, char **raddr, int *rsize)
{
    long ret;
    compressed_kernel_header * kernel_header = (compressed_kernel_header *) binary;
    u_int32_t uncompressed_size, size;
    void *buffer;
	unsigned long len;
	
#if 0
    verbose("kernel header:\n");
    verbose("signature: 0x%x\n", kernel_header->signature);
    verbose("compress_type: 0x%x\n", kernel_header->compress_type);
    verbose("adler32: 0x%x\n", kernel_header->adler32);
    verbose("uncompressed_size: 0x%x\n", kernel_header->uncompressed_size);
    verbose("compressed_size: 0x%x\n", kernel_header->compressed_size);
    getc();
#endif
	
    if (kernel_header->signature == OSSwapBigToHostConstInt32('comp')) {
        if (kernel_header->compress_type != OSSwapBigToHostConstInt32('lzss')) {
            error("kernel compression is bad\n");
            return -1;
        }
#if NOTDEF
        if (kernel_header->platform_name[0] && strcmp(gPlatformName, kernel_header->platform_name))
            return -1;
        if (kernel_header->root_path[0] && strcmp(gBootFile, kernel_header->root_path))
            return -1;
#endif
		
        uncompressed_size = OSSwapBigToHostInt32(kernel_header->uncompressed_size);
        binary = buffer = malloc(uncompressed_size);
		
        size = decompress_lzss((u_int8_t *) binary, &kernel_header->data[0],
                               OSSwapBigToHostInt32(kernel_header->compressed_size));
        if (uncompressed_size != size) {
            error("size mismatch from lzss: %x\n", size);
            return -1;
        }
        if (OSSwapBigToHostInt32(kernel_header->adler32) !=
            Adler32(binary, uncompressed_size)) {
            verbose("adler mismatch\n");
            return -1;
        }
    }
	
	ret = ThinFatFile(&binary, &len);
	if (ret == 0 && len == 0 && archCpuType==CPU_TYPE_X86_64)
	{
		archCpuType=CPU_TYPE_I386;
		ret = ThinFatFile(&binary, &len);
	}
	
	//patch_kernel(binary);
	
	ret = DecodeMachO(binary, rentry, raddr, rsize);
	
	if (ret<0 && archCpuType==CPU_TYPE_X86_64)
	{
		archCpuType=CPU_TYPE_I386;
		ret = DecodeMachO(binary, rentry, raddr, rsize);
	}
	
	
	return ret;
}
