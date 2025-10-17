#include <string>
#include <windows.h>

std::string GetDriverInfo() {
	DWORD driveMask = GetLogicalDrives();

	if(driveMask == 0) {
		return "";
	}
	std::string driveInfo;
	driveInfo.reserve(26 * 3); // Ô¤Áô×ã¹»¿Õ¼ä
	
	for(int i  = 0; i < 26; ++i) {
		if(driveMask & (1 << i)) {
			char driveLetter = 'A' + i;
			driveInfo += driveLetter;
			driveInfo += ":\\ ";
		}
	}
	return driveInfo;
}

