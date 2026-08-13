#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "../include/exec.h"

/*
 * EagleOS built-in executable registry.
 *
 * Every built-in application exposes an executable_header_t
 * through one of these functions.
 */

const executable_header_t *program_manager_executable(void);
const executable_header_t *calculator_executable(void);
const executable_header_t *text_editor_executable(void);
const executable_header_t *file_manager_executable(void);
const executable_header_t *installer_executable(void);
const executable_header_t *sysver_executable(void);
const executable_header_t *settings_executable(void);

#endif /* PROGRAMS_H */