# include  "vpi_user.h"
//# include "sv_vpi_user.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <malloc.h>
 
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>


int search_usage = 0; // 초기화 (1), FF 값 출력(2)
char FF_path[30];

int FF_check_failure = 0;	// FF check 수행중 golden run과 다른 FF가 발견되면 1
int FF_Failure_count =0;	  	// FF_check을 수행하고, Failure가 발생한 개수를 기록한다.

static s_vpi_value check_value_s = {vpiBinStrVal};
static p_vpi_value check_value_p = &check_value_s;

static s_vpi_value Flip_value_s = {vpiBinStrVal};
static p_vpi_value Flip_value_p = &Flip_value_s;

int EX_COUNT =0;

/*

vpiHandle fault_loc_handle;
s_vpi_time 		time_fault_enable[10];
s_vpi_time 		time_fault_release[10];
*/

// 임시 문자열 변수
char str[100];


// 문자열 버퍼를 초기화 하는 함수
void init_str(void)
{
	int i;
	for(i=0; i< 100; i++)
	{
		str[i] = 0;
	
	}

}

///////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 time_check_calltf(char *data)
{
	data =0;
	struct timespec tp;
	int rs;
	rs = clock_gettime(CLOCK_REALTIME, &tp);
	printf("check time : [%ld.%ld] \n", tp.tv_sec, tp.tv_nsec);

	return 0;

}
/////////////////////////////////////////////////////////////////////////////////////////////
// 초기화할 FF의 이름과 초기값을 관리하기 위한 구조체
typedef struct FF_SET
{
	char	FF_name[100];
	int	FF_init_value;

	vpiHandle FF_handle;

}FF_INIT_DB;

// 초기화될 FF와 값을 저장할 배열 변수
FF_INIT_DB FF_DB[20];
// 초기화 될 FF의 전체 개수
int FF_DB_NUM = 0;

// Golden FF로 값을 저장할 배열 변수
FF_INIT_DB FF_Golden[20];
// Golden FF의 전체 개수
int FF_Golden_NUM = 0;


// 결함주입할때 사용활 VPI 함수의 전달 인자값 들 

struct t_cb_data 	cb_FF_INIT_enable[20];
struct t_cb_data 	cb_FF_INIT_release[20];

vpiHandle 		FF_e_h[20];
vpiHandle 		FF_r_h[20];

vpiHandle fault_loc_handle;
s_vpi_time 		time_FF_INIT_enable[20];
s_vpi_time 		time_FF_INIT_release[20];


static s_vpi_value FF_init_value_s = {vpiBinStrVal};
static p_vpi_value FF_init_value_p = &FF_init_value_s;


int FF_enable(p_cb_data cb_data_p)
{
	

	
	int *temp_FF_NUM;
	long FF_NUM;				// sizeof(long) == sizeof(void *)
	char INIT_Value[10]={0};

		
	temp_FF_NUM = (int *)cb_data_p->user_data;
	FF_NUM = (long)temp_FF_NUM;


	if(FF_DB[FF_NUM].FF_init_value == 0)
	{
		INIT_Value[0] = '0';
	}
	else if(FF_DB[FF_NUM].FF_init_value == 1)
	{
		INIT_Value[0] = '1';
	}
	else
	{
		// none
	}

	vpi_get_value(FF_DB[FF_NUM].FF_handle, FF_init_value_p);
	strcpy(FF_init_value_p->value.str, INIT_Value);

//	vpi_put_value(FF_DB[FF_NUM].FF_handle, FF_init_value_p, time_p ,vpiForceFlag);
	vpi_put_value(FF_DB[FF_NUM].FF_handle, FF_init_value_p, NULL ,vpiNoDelay);


	
	return 0;

}

///////////////////////////////////////////////////////////////////////////////////
// 결함 주입을 멈추는 callback 함수

int FF_release(p_cb_data cb_data_p)
{


	cb_data_p = 0;
/*	
	int *xx;
	int x;
	xx = (int *)cb_data_p->user_data;
	
	x= (int)xx;
	
	vpi_put_value(fault_configuration_list[x].fault_handle, value_p, NULL ,vpiReleaseFlag);
*/	
	return 0;
}

static s_vpi_value value_s = {vpiBinStrVal};
static p_vpi_value value_p = &value_s;

//// golden run의 FF 값을 저장한다.
void FF_test_pattern_func(vpiHandle h)
{
	char INIT_Value[10]={0};
//vpi_printf("aaa %d \n", FF_Golden_NUM);	
	for(int i=0; i< FF_Golden_NUM ; i++)
	{


		if(strcmp(vpi_get_str(vpiFullName , h), FF_DB[i].FF_name) == 0)
		{

			if(strcmp(FF_DB[i].FF_name, FF_path)== 0 )
			{// 결함이 주입되는 FF

				if(FF_DB[i].FF_init_value == 0)
				{
					INIT_Value[0] = '1';
				}
				else if(FF_DB[i].FF_init_value == 1)
				{
					INIT_Value[0] = '0';
				}
				else
				{
					// none
				}

			}
			else
			{
			// 정상값이 적용되는 FF
				if(FF_DB[i].FF_init_value == 0)
				{
					INIT_Value[0] = '0';
				}
				else if(FF_DB[i].FF_init_value == 1)
				{
					INIT_Value[0] = '1';
				}
				else
				{
					// none
				}



			}

			vpi_get_value(FF_DB[i].FF_handle, FF_init_value_p);
			strcpy(FF_init_value_p->value.str, INIT_Value);

			vpi_put_value(FF_DB[i].FF_handle, FF_init_value_p, NULL ,vpiNoDelay);


			
		}
	}
}


//// golden run의 FF 값을 저장한다.
void FF_golden_func(vpiHandle h)
{
	for(int i=0; i< FF_DB_NUM ; i++)
	{
		if(strcmp(vpi_get_str(vpiFullName , h), FF_DB[i].FF_name) == 0)
		{
			vpi_get_value(h, value_p);
			strcpy(FF_Golden[FF_Golden_NUM].FF_name, FF_DB[i].FF_name);
			FF_Golden[FF_Golden_NUM].FF_init_value = atoi(value_p->value.str);
			FF_Golden_NUM++;
			//vpi_printf(" ==> %s		%s\n", vpi_get_str(vpiFullName , h), value_p->value.str);
			return;
			
		}
	}
}

void FF_check_func(vpiHandle h)
{


	for(int i=0; i< FF_DB_NUM ; i++)
	{

//vpi_printf("%s, %s \n", vpi_get_str(vpiFullName , h), FF_Golden[i].FF_name);

//		if((strcmp(vpi_get_str(vpiFullName , h), FF_Golden[i].FF_name) == 0) &&
//			(strcmp(vpi_get_str(vpiFullName , h), FF_path) != 0))
		if(strcmp(vpi_get_str(vpiFullName , h), FF_Golden[i].FF_name) == 0)
		{
			vpi_get_value(h, value_p);
			
			vpi_printf("%s	: golden : %d	: EX : %d \n",FF_Golden[i].FF_name, FF_Golden[i].FF_init_value, atoi(value_p->value.str) );
			

			if(atoi(value_p->value.str) != FF_Golden[i].FF_init_value)
			{
				FF_check_failure =1;
			}

			return;
			
		}
	}
}

void FF_current_check(vpiHandle h)
{
	for(int i=0; i< FF_DB_NUM ; i++)
	{
		if(strcmp(vpi_get_str(vpiFullName , h), FF_DB[i].FF_name) == 0)
		{
			vpi_get_value(h, value_p);
			vpi_printf(" ==> %s		%s\n", vpi_get_str(vpiFullName , h), value_p->value.str);
			return;
			
		}
	}
}

void FF_Flip(vpiHandle h)
{

	for(int i=0; i< FF_DB_NUM ; i++)
	{
		if(strcmp(vpi_get_str(vpiFullName , h), FF_path) == 0)
		{
			// 초기화 할 FF을 찾은 후에, FF의 PLI handle 값을 받아온다.
			


			vpi_get_value(h, check_value_p);
			vpi_get_value(h, Flip_value_p);

			vpi_printf("=====%c \n", check_value_p->value.str[0]);

			if(check_value_p->value.str[0] == '0')
			{
 				Flip_value_p->value.str[0] = '1';
			}
			if(check_value_p->value.str[0] == '1')
			{

 				Flip_value_p->value.str[0] = '0';
			}
			else
			{
				vpi_printf("Can not Flip in FF\n");
				return;
 				
			}

			vpi_put_value(h, Flip_value_p, NULL ,vpiNoDelay);


			return;
			
		}
	}

}

void FF_init_check(vpiHandle h)
{

	for(int i=0; i< FF_DB_NUM ; i++)
	{
		if(strcmp(vpi_get_str(vpiFullName , h), FF_DB[i].FF_name) == 0)
		{
			// 초기화 할 FF을 찾은 후에, FF의 PLI handle 값을 받아온다.
			FF_DB[i].FF_handle =h;
			return;
			
		}
	}

}

void FF_clean(vpiHandle h)
{


	char INIT_Value[10]={0};
	INIT_Value[0] = 'x';



	for(int i=0; i< FF_DB_NUM ; i++)
	{


		if(strcmp(vpi_get_str(vpiFullName , h), FF_DB[i].FF_name) == 0)
		{
			vpi_get_value(h, FF_init_value_p);
			strcpy(FF_init_value_p->value.str, INIT_Value);
			
			vpi_put_value(h, FF_init_value_p, NULL ,vpiNoDelay);
			
		}
	}

}


///////////////////////////////////////////////////////////////////////////////////////////
// 시뮬레이션 모델 내에 net 형 데이터를 탐색하는 함수

void search_FF_net(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;


	itr = vpi_iterate(vpiNet, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module), vpiPort
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		if(search_usage == 1)
			FF_init_check(find_handle);
		else if(search_usage ==2)
			FF_current_check(find_handle);
		else if(search_usage ==3)
			FF_clean(find_handle);
		else if(search_usage ==4)
			FF_Flip(find_handle);
		else if(search_usage ==5)
			FF_golden_func(find_handle);
		else if(search_usage ==6)
			FF_test_pattern_func(find_handle);
		else if(search_usage ==7)
			FF_check_func(find_handle);
		else
			;
	}
	
}

///////////////////////////////////////////////////////////////////////////////////////////
// 시뮬레이션 모델 내에 reg 형 데이터를 탐색하는 함수

void search_FF_reg(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;
	
	itr = vpi_iterate(vpiReg, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		if(search_usage == 1)
			FF_init_check(find_handle);
		else if(search_usage ==2)
			FF_current_check(find_handle);
		else if(search_usage ==3)
			FF_clean(find_handle);
		else if(search_usage ==4)
			FF_Flip(find_handle);
		else if(search_usage ==5)
			FF_golden_func(find_handle);
		else if(search_usage ==6)
			FF_test_pattern_func(find_handle);
		else if(search_usage ==7)
			FF_check_func(find_handle);
		else
			;
	}

}

///////////////////////////////////////////////////////////////////////////////////////////
// 시뮬레이션 모델 내에 module 를 탐색하는 함수

void search_FF_module(vpiHandle root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;

			
	itr = vpi_iterate(vpiModule, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	

	if(itr == NULL)		// 최 하위 모듈 -- 더이상의 하위 계층의 모듈은 존재하지 않는다.
	{
		search_FF_net(root_handle);
		search_FF_reg(root_handle);
		return;
	}

		
	while( (find_handle = vpi_scan(itr)))
	{

		search_FF_net(find_handle);
		search_FF_reg(find_handle);
		
		search_FF_module(find_handle);
	}
}

// 모델 내부의 FF의 값을 초기화 하기위한 UST
static PLI_INT32 FF_init_calltf(char *data)
{
//	vpi_printf("\n\n FF Initial !!!\n");
	data =0;

	// 초기화할 FF list을 저장하는 파일로 "<FF name><\t><init value><\n> 형태로 구성됨
	FILE *FF_fault_file = fopen("fault-list-FF", "rt");
	char ch;
	int str_num=0;	

	// FF list을 모두 탐색할 때까지 무한 루프
	while(1)
	{

		ch = fgetc(FF_fault_file);
		
		// 다음 문자열 직전에는 초기화할 값이 저장된다.
		if(ch == '\n')
		{
			FF_DB[FF_DB_NUM].FF_init_value = atoi(str);
			init_str();
			FF_DB_NUM++;
			str_num =0;
		}
		// 탭 직전에는 초기화할 FF의 이름이 저장된다.
		else if(ch == '\t')
		{
			strcpy(FF_DB[FF_DB_NUM].FF_name, str);	
			init_str();
			str_num =0;

		}	
		else 
		{
			str[str_num++] = ch;
			
		}


		if(feof(FF_fault_file) != 0)
			break;

	}
	

/*
	for(int i =0; i< FF_DB_NUM ; i++)
	{
		vpi_printf("%s\t\t%d\n", FF_DB[i].FF_name,FF_DB[i].FF_init_value);
	}
*/
	fclose(FF_fault_file);

	// 

      //초기화 할 대상FF의 handle을 찾기 위한 구문
	vpiHandle top_module;
	
	top_module= vpi_handle_by_name("tb_s344", NULL);
	
	vpi_printf(" ----------->  TopModule	: %s \n", vpi_get_str(vpiFullName, top_module));


	// target 내부의 모든 module, net, reg 객체를 탐색
	search_usage	= 1;
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 0;

	// FF을 초기화 하기 위한 callback 함수를 등록...
      // FF 초기화시간은 10ns, 해제시간은 20ns  --> 1000

	for(int i = 0; i<FF_DB_NUM ; i++) 
	{
		time_FF_INIT_enable[i].high 		= 0;
		time_FF_INIT_enable[i].low		= 0;//fault_configuration_list[i].fault_injection_time;
		time_FF_INIT_enable[i].type 		= vpiSimTime;
		
		cb_FF_INIT_enable[i].reason		= cbReadWriteSynch;
		cb_FF_INIT_enable[i].cb_rtn		= FF_enable;
		cb_FF_INIT_enable[i].time 		= &time_FF_INIT_enable[i];
		cb_FF_INIT_enable[i].obj			= FF_e_h[i];
		cb_FF_INIT_enable[i].value 		= 0;
		cb_FF_INIT_enable[i].user_data		= (PLI_INT32 *)(i);

		

		time_FF_INIT_release[i].high 		= 0;
		time_FF_INIT_release[i].low		= 1000;
		time_FF_INIT_release[i].type 		= vpiSimTime;
		
		cb_FF_INIT_release[i].reason		= cbReadWriteSynch;
		cb_FF_INIT_release[i].cb_rtn		= FF_release;
		cb_FF_INIT_release[i].time 		= &time_FF_INIT_release[i];
		cb_FF_INIT_release[i].obj		= FF_r_h[i];
		cb_FF_INIT_release[i].value 		= 0;
		cb_FF_INIT_release[i].user_data		= (PLI_INT32 *)(i);	
		
		vpi_register_cb(&cb_FF_INIT_enable[i]);
		vpi_register_cb(&cb_FF_INIT_release[i]);

	
	}


	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 FF_current_calltf(char *data)
{
	vpi_printf("\n\n FF_current ...\n");
	data =0;

	// FF의 D 값을 출력하는 기능을 수행
	// FF List에서 FF D의 값을 확인하고, 
	// Scerch 루틴을 돌리고,,,
	// FF 목록에 있는 FF의 값을 출력한다.

	
	// target 내부의 모든 module, net, reg 객체를 탐색
	vpiHandle top_module;
	top_module= vpi_handle_by_name("tb_s344", NULL);

	search_usage	= 2;	
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 0;


	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 FF_clean_calltf(char *data)
{
//	vpi_printf("\n\n FF_clean ...\n");
	data =0;

	// target 내부의 모든 module, net, reg 객체를 탐색
	vpiHandle top_module;
	top_module= vpi_handle_by_name("tb_s344", NULL);

	search_usage	= 3;	
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 0;


	FF_Golden_NUM = FF_DB_NUM;

	//FF_DB_NUM =0;


	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////





static PLI_INT32 FF_test_pattern_calltf(char *data)
{
//	vpi_printf("\n FF_test pattern ...\n");
	EX_COUNT++;
	data =0;

	// 파라미터 값을 읽어온다.	
	vpiHandle systfref, args_iter, argh;
  	struct t_vpi_value argval;

	// Obtain a handle to the argument list
	systfref = vpi_handle(vpiSysTfCall, NULL);
	args_iter = vpi_iterate(vpiArgument, systfref);


	// Grab the value of the first argument
	argh = vpi_scan(args_iter);
	argval.format = vpiStringVal;
	vpi_get_value(argh, &argval);
	strcpy(FF_path, argval.value.str);


	vpiHandle top_module;
	top_module= vpi_handle_by_name("tb_s344", NULL);

	search_usage	= 6;	
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 6;




	return 0;
}

static PLI_INT32 FF_bitflip_calltf(char *data)
{
	vpi_printf("\n FF_bitflip ...\n");
	data =0;

	// 파라미터 값을 읽어온다.	
	vpiHandle systfref, args_iter, argh;
  	struct t_vpi_value argval;

	// Obtain a handle to the argument list
	systfref = vpi_handle(vpiSysTfCall, NULL);
	args_iter = vpi_iterate(vpiArgument, systfref);

	// Grab the value of the first argument
	argh = vpi_scan(args_iter);
	argval.format = vpiStringVal;
	vpi_get_value(argh, &argval);
	strcpy(FF_path, argval.value.str);


	vpiHandle top_module;
	top_module= vpi_handle_by_name("tb_s344", NULL);

	search_usage	= 4;	
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 0;



/*
	vpi_get_value(FF_module, check_value_p);

	// 값 을 확인한후, flip 값을 설정한다.
	if(strcmp(check_value_p->value.str, "0") == 0)
	{
		strcpy(Flip_value_p, "1");
	}
	else if(strcmp(check_value_p->value.str, "1") == 0)
	{
		strcpy(Flip_value_p, "0");
	}
	else
	{

	}




	// flip 값을 넣어준다.

	vpi_put_value(FF_module, Flip_value_p, NULL ,vpiNoDelay);
*/

	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////


static PLI_INT32 FF_golden_dump_calltf(char *data)
{
//	vpi_printf("\n FF_golden dump ...\n");
	data =0;


	// target 내부의 모든 module, net, reg 객체를 탐색
	vpiHandle top_module;
	top_module= vpi_handle_by_name("tb_s344", NULL);

	search_usage	= 5;	
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 0;




	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


static PLI_INT32 Report_calltf(char *data)
{


	vpi_printf("\n *************** Result **********************\n");
	vpi_printf("*** FF Failure Count : %d              *********\n", FF_Failure_count);
	vpi_printf("************************************************\n");

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 FF_check_calltf(char *data)
{
	vpi_printf("%d 's TEST -----------!!!\n", EX_COUNT);

	data =0;


	// target 내부의 모든 module, net, reg 객체를 탐색
	vpiHandle top_module;
	top_module= vpi_handle_by_name("tb_s344", NULL);

	search_usage	= 7;	
	search_FF_net(top_module);
	search_FF_reg(top_module);
	search_FF_module(top_module);
	search_usage	= 7;


	if( FF_check_failure == 1)
	{	// 하나의 FF 이라도 오류가 발견되었다.
		FF_Failure_count++;
		vpi_printf("Failure !!!!!! \n");
	}
	else
	{
		vpi_printf("Sucess !!!!!! \n");
	}

	FF_check_failure =0;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void time_check_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$time_check";
      tf_data.calltf    = time_check_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}
////////////////////////////////////////////////////////////////////////////////////////////////////
void FF_init_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_init";
      tf_data.calltf    = FF_init_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void FF_current_state_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_current_state";
      tf_data.calltf    = FF_current_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FF_clean_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_clean_state";
      tf_data.calltf    = FF_clean_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void FF_bitflip_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_bitflip";
      tf_data.calltf    = FF_bitflip_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FF의 golden data 상태를 저장하는 systam task.

void FF_golden_dump_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_golden_dump";
      tf_data.calltf    = FF_golden_dump_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FF의 결함 값이 적용된 test pattern을 적용하는 system task.

void FF_test_pattern_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_test_pattern";
      tf_data.calltf    = FF_test_pattern_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FF의 현재 상태와 golden run으로 저장해 놓은 상태를 비교하여 결과를 저장한다.

void FF_check_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$FF_check";
      tf_data.calltf    = FF_check_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FF의 시험결과를 확인하기 위한 system task

void Report_register(void)
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$Report";
      tf_data.calltf    = Report_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void (*vlog_startup_routines[100])(void) =
{
  time_check_register,
  FF_init_register,
  FF_current_state_register,
  FF_clean_register,
  FF_bitflip_register,
  FF_golden_dump_register,
  FF_test_pattern_register,
  FF_check_register,
  Report_register,
  0 /*** final entry must be 0 ***/
};

