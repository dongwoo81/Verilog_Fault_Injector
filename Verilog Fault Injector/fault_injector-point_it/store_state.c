/*

// 2011 9 7 --> Partial Fault Injection 구현 
// 2011 11 28 --> Multi-bit fault injection 구현

VPI FI Processor version 1.0  ->   2012. 1. 5
s
프로세서 모델의 가속 결함주입 실험을 실시하기 위한 verilog system task

지원 system task

1. $ICARUS_FI  =>  



*/



#define ICARUS		0
#define Modelsim	1


#ifdef HAVE_CVS_IDENT
#ident "$Id: hello_vpi.c,v 1.5 2007/01/17 05:35:48 steve Exp $"
#endif

# define VPI_COMPATIBILITY_VERSION_1364v2001 1

# include  "vpi_user.h"
//# include "sv_vpi_user.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <malloc.h>

extern int snapshot_interval;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Snap Save
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct t_cb_data 	snap_save_callback;		// 차후에 구조체가 될 수 있음
vpiHandle 		s_save_h;
s_vpi_time 		snap_save_enable;		// snap shot save time
vpiHandle top_save_module;

int snap_save_seek =0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
char priv_buffer[100];

//////////////////////////////////////////////////////////////////////////////////////////////////////

#define check_num 70

FILE *snap_save_DB; 

//////////////////////////////////////////////////////////////////////////////////////////////////////
static s_vpi_value value_save_s = {vpiBinStrVal};
static p_vpi_value value_save_p = &value_save_s;

int snap_save(p_cb_data cb_data_p);


void next_simulation(int time)
{

	snap_save_enable.high 			= 0;
	snap_save_enable.low			= time;	//veriable factor
	snap_save_enable.type 			= vpiSimTime;
	
	snap_save_callback.reason		= cbAfterDelay;
	snap_save_callback.cb_rtn		= snap_save;
	snap_save_callback.time 		= &snap_save_enable;
	snap_save_callback.obj			= s_save_h;
	snap_save_callback.value 		= 0;
	snap_save_callback.user_data		= (PLI_BYTE8 *)1;
			
	////////////////////////////////////////////////////////////////////////		

	vpi_register_cb(&snap_save_callback);



}



void search_save_net(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;
	
	itr = vpi_iterate(vpiNet, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
	
		vpi_get_value(find_handle, value_save_p);
		fputs(vpi_get_str(vpiName,find_handle) ,snap_save_DB);
		fputc(' : ', snap_save_DB);
		fputs(value_save_p->value.str,snap_save_DB);fputc('\n', snap_save_DB);

		
	}
	
}

void search_save_MemoryWord(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;

	//vpiRegBit, vpiMemory, vpiMemoryWord
	itr = vpi_iterate(vpiMemoryWord, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		vpi_get_value(find_handle, value_save_p);
		fputs(value_save_p->value.str,snap_save_DB);fputc('\n', snap_save_DB);

	}


}

void search_save_Memory(vpiHandle  root_handle)
{

	vpiHandle itr;
	vpiHandle find_handle;


	itr = vpi_iterate(vpiMemory, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{

		
		search_save_MemoryWord(find_handle);


	}
	
}


void search_save_reg(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;

	//vpiRegBit, vpiMemory, vpiMemoryWord
	itr = vpi_iterate(vpiReg, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)

	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		
		vpi_get_value(find_handle, value_save_p);
		fputs(value_save_p->value.str, snap_save_DB);fputc('\n', snap_save_DB);


	}
	
	

}


void search_save_module(vpiHandle root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;

			
	itr = vpi_iterate(vpiModule, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
	{
		return;
	}
		
	while( (find_handle = vpi_scan(itr)))
	{
		search_save_net(find_handle);
//		search_save_reg(find_handle);
//		search_save_Memory(find_handle);
		search_save_module(find_handle);
	}
	
	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
char buff[100];

int save_time = 1;

int snap_save(p_cb_data cb_data_p)
{


	vpi_printf(" -------> Snap Shot enable time \n\n");

	sprintf(buff, "Snap_DB");
	snap_save_DB = fopen(buff, "at");
	
	
	sprintf(buff, "T : %d", save_time * snapshot_interval);


	fputs(buff, snap_save_DB);fputc('\n', snap_save_DB);


//struct timespec tp;
//int rs;
//rs = clock_gettime(CLOCK_REALTIME, &tp);
//printf("[%ld.%ld]             ", tp.tv_sec, tp.tv_nsec);

	
	search_save_net(top_save_module);
//	search_save_reg(top_save_module);
//	search_save_Memory(top_save_module);
	search_save_module(top_save_module);

//rs = clock_gettime(CLOCK_REALTIME, &tp);
//printf("[%ld.%ld] \n", tp.tv_sec, tp.tv_nsec);
	
	
	fclose(snap_save_DB);
	snap_save_seek++;


	save_time++;
	next_simulation(snapshot_interval);

	return 0;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 snap_save_calltf(char *data)
{
      	vpi_printf("\n\n Snap Save, from VPI.- FULL\n");
	int checkpoint_time;



//	int i;
//	for(i = 1; i <= check_num ; i++)
//	{
		checkpoint_time = 1 * snapshot_interval;
		
  
     
      // 모델 탐색 및 오류주입 위치 핸들 수신
	
		top_save_module= vpi_handle_by_name("or1200_tb", NULL);

  
		snap_save_enable.high 			= 0;
		snap_save_enable.low			= checkpoint_time;	//veriable factor
		snap_save_enable.type 			= vpiSimTime;
		
		snap_save_callback.reason		= cbAfterDelay;
		snap_save_callback.cb_rtn		= snap_save;
		snap_save_callback.time 		= &snap_save_enable;
		snap_save_callback.obj			= s_save_h;
		snap_save_callback.value 		= 0;
		snap_save_callback.user_data		= (PLI_BYTE8 *)1;
			
	////////////////////////////////////////////////////////////////////////		

		vpi_register_cb(&snap_save_callback);
//	}
      
      return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////

void  snap_save_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$snap_save";
      tf_data.calltf    = snap_save_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}

