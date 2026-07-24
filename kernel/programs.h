// EagleOS 1.0 Program Registry Declarations.
#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "../include/exec.h"

const executable_header_t *program_manager_executable(void);
const executable_header_t *calculator_executable(void);
const executable_header_t *text_editor_executable(void);
const executable_header_t *file_manager_executable(void);
const executable_header_t *installer_executable(void);
const executable_header_t *sysver_executable(void);

#endif // PROGRAMS_H
