/** @file messages.h
 *  @brief text messages helpers
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 23-Mar-2019
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#ifdef __cplusplus
 extern "C" {
#endif

//extern char *params_load;
extern char *params_loaded;
extern char *params_not_loaded;

extern char *error_saving;
//extern char *cfg_file;

void messages_Task_started(void);

void messages_TaskInit_started(void);

void messages_TaskInit_OK(void);

void messages_TaskInit_fail(void);

#ifdef __cplusplus
 }
#endif


#endif // MESSAGES_H
