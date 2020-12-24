#include <stdio.h>
#include <string.h>
#include <ppu-lv2.h>
#include <lv2/sysfs.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <io/pad.h>
#include <fcntl.h>

/* #include "category_user_bin.h"
#include "category_user_login_bin.h"
#include "xai_plugin_hard_bin.h"
#include "xai_plugin_soft_bin.h" */

#include "lv2_utils.h"

#define FS_S_IFMT 0170000

int dev_rw_mounted = 0;
sysFSStat stat1;
char buffer[65536];

uint8_t dex_flash = 0;
uint8_t dex_mode = 0;
static float c_firmware=0.0f;

static int sys_fs_mount_ext(char const* deviceName, char const* deviceFileSystem, char const* devicePath, int writeProt, u32* buffer, u32 count) 
{
    lv2syscall8(837, (u64) deviceName, (u64) deviceFileSystem, (u64) devicePath, 0ULL, (u64) writeProt, 0ULL, (u64) buffer, (u64) count);
    return_to_user_prog(int);
}

static int sys_fs_umount(char const* devicePath) 
{
    lv2syscall3(838,  (u64) devicePath, 0, 0 );
    return_to_user_prog(int);
}

static int filestat(const char *path, sysFSStat *stat)
{
    int ret = sysLv2FsStat(path, stat);

    if(ret == 0 && S_ISDIR(stat->st_mode)) return -1;
    
    return ret;
}

static int unlink_secure(void *path)
{
    sysFSStat s;
    if(filestat(path, &s)>=0) {
        sysLv2FsChmod(path, FS_S_IFMT | 0777);
        return sysLv2FsUnlink(path);
    }
    return -1;
}

static int mount_flash()
{
	if(dev_rw_mounted || (!dev_rw_mounted && sys_fs_mount_ext("CELL_FS_IOS:BUILTIN_FLSH1", "CELL_FS_FAT", "/dev_rebug", 0, NULL, 0)==0))
	{
        dev_rw_mounted = 1;
    }

    return dev_rw_mounted;
}

static int file_copy(const char *src, const char *dst)
{
    // sysFSStat stat1;
    int fd, fd2;
    int ret;
    u64 temp = 0;
    u64 readed = 0;

    if(filestat(src, &stat1)!=0 || stat1.st_size == 0) return -1;

    if(!sysLv2FsOpen(src, SYS_O_RDONLY, &fd, 0, NULL, 0))
	{
        if(!sysLv2FsOpen(dst, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, &fd2, 0777, NULL, 0))
		{
            sysLv2FsChmod(dst, FS_S_IFMT | 0777);

            while(stat1.st_size != 0ULL)
			{
                readed = stat1.st_size;
                if(readed > 65536) readed = 65536;
                temp = 0;
                ret = sysLv2FsRead(fd, buffer, readed, &temp);
                if(ret < 0 || readed != temp) break;
                ret = sysLv2FsWrite(fd2, buffer, readed, &temp);
                if(ret < 0 || readed != temp) break;

                stat1.st_size -= readed;
            }

            sysLv2FsClose(fd);
            sysLv2FsClose(fd2);

            if(stat1.st_size) return -4;
        } 
		else 
		{
            sysLv2FsClose(fd);
            return -3;
        }
    } 
	else return -2;

    return 0;
}

static void detect_firmware(void)
{
	uint64_t CEX=0x4345580000000000ULL;
	uint64_t DEX=0x4445580000000000ULL;

/* 	if(lv2peek(0x80000000002E8610ULL)==CEX) { dex_mode=0; c_firmware=4.21f; } else
	if(lv2peek(0x8000000000302D88ULL)==DEX) { dex_mode=2; c_firmware=4.21f; } else
	if(lv2peek(0x80000000002ED818ULL)==CEX) { dex_mode=0; c_firmware=(lv2peek(0x80000000002FCB68ULL)==0x323031372F30382FULL) ? 4.82f : (lv2peek(0x80000000002FCB68ULL)==0x323031352F31322FULL) ? 4.78f;} else
	if(lv2peek(0x800000000030F2D0ULL)==DEX) { dex_mode=2; c_firmware=4.75f; } else
	if(lv2peek(0x800000000030F3B0ULL)==DEX) { dex_mode=2; c_firmware=4.81f; } */
	if(lv2peek(0x80000000002E8610ULL)==CEX) { c_firmware=4.21f; dex_mode=0; } else
	if(lv2peek(0x8000000000302D88ULL)==DEX) { c_firmware=4.21f; dex_mode=2; } else
	if(lv2peek(0x80000000002ED818ULL) == CEX) { c_firmware = (lv2peek(0x80000000002FCB68ULL) == 0x323031372F30382FULL) ? 4.82f : (lv2peek(0x80000000002FCB68ULL) == 0x323031362F31302FULL) ? 4.81f : (lv2peek(0x80000000002FCB68ULL) == 0x323031352F31322FULL) ? 4.78f : (lv2peek(0x80000000002FCB68ULL) == 0x323031352F30382FULL) ? 4.76f : 4.75f; dex_mode=0; } else
	if(lv2peek(0x800000000030F2D0ULL)==DEX) { c_firmware=4.75f; dex_mode=2; } else
	if(lv2peek(0x800000000030F3B0ULL)==DEX) { c_firmware=4.81f; dex_mode=2; }
}
// static int detect_target(void)
/* static void detect_target(void)
{
	uint64_t type;
	{lv2syscall1(985, (uint64_t)(uint32_t) &type);};
	// lv2syscall1(985, (u32*)&type);
	if(type==2) dex_flash = 1;//DEX
	// return (int)(type - 1);
} */

int main()
{
	padInfo padinfo;
	padData paddata;
	int i;

	ioPadInit(7);

	int is_rebug = sysLv2FsStat("/dev_flash/rebug", &stat1);

	if(is_rebug==0)
	{
		int is_rbgrw = sysLv2FsStat("/dev_rebug", &stat1);
		if(is_rbgrw==0)
		{
			sys_fs_umount("/dev_rebug");
		}

		int is_bldrw = sysLv2FsStat("/dev_blind", &stat1);
		if(is_bldrw==0)
		{
			sys_fs_umount("/dev_blind");
		}

		if(!dev_rw_mounted)
		{
			if(mount_flash())
			{
install:
				if(filestat("/dev_flash/vsh/module/xai_plugin.sprx", &stat1)==0 
				&& (filestat("/dev_flash/vsh/resource/explore/xmb/cfw_settings.xml", &stat1)==0 
				|| filestat("/dev_flash/vsh/resource/explore/xmb/cfw_settings_en.xml", &stat1)==0))
				{
						detect_firmware();
						if (dex_mode)
						{
							if(filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp", &stat1)!=0
							&& filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", &stat1)!=0)
							{
								file_copy("/dev_rebug/vsh/module/xai_plugin.sprx", "/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp");

								if (c_firmware == 4.81f || c_firmware == 4.21f)
								{
									file_copy("/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml", "/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp");
								}
								if (c_firmware == 4.75f)
								{
									file_copy("/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml", "/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp");
								}
							}

							if(filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/CFG", &stat1)==0)
								unlink_secure("/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");

							if (c_firmware == 4.75f)
							{
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/478/478DEX", "/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");
							}
							else
							if (c_firmware == 4.81f)
							{
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/482/482DEX", "/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");
							}
							else
							if (c_firmware == 4.21f)
							{
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/421/421DEX", "/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");
							}

							ioPadGetInfo(&padinfo);
							for(i = 0; i < MAX_PADS; i++)
							{
								if(padinfo.status[i])
								{
									ioPadGetData(i, &paddata);
									if(paddata.BTN_START) // hold START to install backup files
									{
										if(filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp", &stat1)==0
										&& filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", &stat1)==0)
										{
											unlink_secure("/dev_rebug/vsh/module/xai_plugin.sprx");
											file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp", "/dev_rebug/vsh/module/xai_plugin.sprx");

											if (c_firmware == 4.81f || c_firmware == 4.21f)
											{
												unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
												file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
											}
											if (c_firmware == 4.75f)
											{
												unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
												file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
											}
										}

										lv2syscall3(392, 0x1004, 0x7, 0x36);//2xBeep
										goto exit;
									}
								}
							}

							unlink_secure("/dev_rebug/vsh/module/xai_plugin.sprx");
							file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/xai_plugin.sprx.new", "/dev_rebug/vsh/module/xai_plugin.sprx");

							if (c_firmware == 4.81f ||  c_firmware == 4.21f)
							{
								unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/cfw_settings.xml.new", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
							}
							if (c_firmware == 4.75f)
							{
								unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/cfw_settings.xml.new", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
							}

							lv2syscall3(392, 0x1004, 0x4, 0x6);//1xBeep
						}
						else
						{
							if(filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp", &stat1)!=0
							&& filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", &stat1)!=0)
							{
								file_copy("/dev_rebug/vsh/module/xai_plugin.sprx", "/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp");

								if (c_firmware == 4.82f || c_firmware == 4.21f)
								{
									file_copy("/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml", "/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp");
								}
								if (c_firmware == 4.78f)
								{
									file_copy("/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml", "/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp");
								}
							}

							if(filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/CFG", &stat1)==0)
								unlink_secure("/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");

							if (c_firmware == 4.78f)
							{
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/478/478CEX", "/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");
							}
							else
							if (c_firmware == 4.82f)
							{
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/482/482CEX", "/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");
							}
							else
							if (c_firmware == 4.21f)
							{
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/421/421CEX", "/dev_hdd0/game/CCAPILOAD/USRDIR/CFG");
							}

							ioPadGetInfo(&padinfo);
							for(i = 0; i < MAX_PADS; i++)
							{
								if(padinfo.status[i])
								{
									ioPadGetData(i, &paddata);
									if(paddata.BTN_START) // hold START to install backup files
									{
										if(filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp", &stat1)==0
										&& filestat("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", &stat1)==0)
										{
											unlink_secure("/dev_rebug/vsh/module/xai_plugin.sprx");
											file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/xai_plugin.sprx.bkp", "/dev_rebug/vsh/module/xai_plugin.sprx");

											if (c_firmware == 4.82f || c_firmware == 4.21f)
											{
												unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
												file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
											}
											if (c_firmware == 4.78f)
											{
												unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
												file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/BACKUP/cfw_settings.xml.bkp", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
											}
										}

										lv2syscall3(392, 0x1004, 0x7, 0x36);//2xBeep
										goto exit;
									}
								}
							}

							unlink_secure("/dev_rebug/vsh/module/xai_plugin.sprx");
							file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/xai_plugin.sprx.new", "/dev_rebug/vsh/module/xai_plugin.sprx");

							if (c_firmware == 4.82f ||  c_firmware == 4.21f)
							{
								unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/cfw_settings.xml.new", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings.xml");
							}
							if (c_firmware == 4.78f)
							{
								unlink_secure("/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
								file_copy("/dev_hdd0/game/CCAPILOAD/USRDIR/cfw_settings.xml.new", "/dev_rebug/vsh/resource/explore/xmb/cfw_settings_en.xml");
							}

							lv2syscall3(392, 0x1004, 0x4, 0x6);//1xBeep
						}
				}

				else goto exit;
			}
		}

		else goto install;
	}

exit:
    ioPadEnd();

	if(dev_rw_mounted)
	{
		sys_fs_umount("/dev_rebug");
		dev_rw_mounted = 0;
	}

	return 0;
}
