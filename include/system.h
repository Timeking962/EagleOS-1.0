// EagleOS 1.0 system control API.
#ifndef SYSTEM_H
#define SYSTEM_H

void system_reboot(void);
const char *system_get_version(void);
const char *system_get_build_tag(void);
const char *GetVersion(void);
const char *GetBuildTag(void);

#endif // SYSTEM_H
