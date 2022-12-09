/*
 * |-----------------------------------------------------------------------|
 * |                                                                       |
 * |   Copyright Cadence Design Systems, Inc. 1985, 1988.                  |
 * |     All Rights Reserved.       Licensed Software.                     |
 * |                                                                       |
 * |                                                                       |
 * | THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF CADENCE DESIGN SYSTEMS |
 * | The copyright notice above does not evidence any actual or intended   |
 * | publication of such source code.                                      |
 * |                                                                       |
 * |-----------------------------------------------------------------------|
 */

/*
 * |-------------------------------------------------------------|
 * |                                                             |
 * | PROPRIETARY INFORMATION, PROPERTY OF CADENCE DESIGN SYSTEMS |
 * |                                                             |
 * |-------------------------------------------------------------|
 */

#include "vpi_user.h"
//#include "vpi_user_cds.h"


extern void ICARUS_FI_register();
extern void snap_save_register();
extern void snap_load_register();
extern void golden_sim_register();
extern void fault_sim_register();
extern void fault_collapsing_register();
extern void time_check_register();
//extern void ACE_MAP_register();

void (*vlog_startup_routines[100])() =
{
  ICARUS_FI_register,
  MODEL_ANLYSIS_register,
  0 /*** final entry must be 0 ***/
};

