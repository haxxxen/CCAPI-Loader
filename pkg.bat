@echo Content-ID = UP0001-CCAPILOAD_00-0000000000000000>_npdrm.conf
@echo klicensee = 0x00000000000000000000000000000000>>_npdrm.conf
@echo DRMType = Free>>_npdrm.conf
@echo ContentType = Game_Exec>>_npdrm.conf
@echo PackageVersion = 01.00>>_npdrm.conf
_npdrm _npdrm.conf UP0001-CCAPILOAD_00-0000000000000000
move UP0001-CCAPILOAD_00-0000000000000000.pkg ccapi.pkg
del _npdrm.conf
pause

