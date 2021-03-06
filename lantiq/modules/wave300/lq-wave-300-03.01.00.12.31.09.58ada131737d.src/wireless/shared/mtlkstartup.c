/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */
/* Helper code and data for startup and  */
/* shutdown control module               */
#include "mtlkinc.h"
#include "mtlkstartup.h"

/* Startup steps counter */
int g_CurrentStepNumber = 0;
void mtlk_startup_clear_step_counter(void)
{
  g_CurrentStepNumber = 0;
}

#ifdef MTLK_DEBUG

/* Failures simulation:                               */
/* This variable is set to the number of step that    */
/* have to fail (simulation of failure for testing)   */
/* Default value of 0 means do not simulate failures. */
int g_StepNumberToFail = 0;
void mtlk_startup_set_step_to_fail(uint32 step)
{
  g_StepNumberToFail = step;
}

#endif
