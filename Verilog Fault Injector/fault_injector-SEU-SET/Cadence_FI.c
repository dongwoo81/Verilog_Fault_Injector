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


int snapshot_interval;
// /////////////////////////////////////////////////////////////////////////////////////////////////
// fault collapsing variable
// /////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct fault_location_list_for_fault_collapsing
{
	char list[1000][150];
	int list_count;
	char fault_location[150];




}fault_location_list;

fault_location_list FL_list;
int fault_collapsing_set =0;


// /////////////////////////////////////////////////////////////////////////////////////////////////
// Concurrent Simulation variable
// /////////////////////////////////////////////////////////////////////////////////////////////////

int network_config();
void net_config_enroll();

void *receiving_thread(void *arg);
void stop_simulation(int time);
// 병렬처리에 필요한 소켓, 스레트 변수는 모두 전역으로 구축한다..
int Masking_check = 0;	// Load checkpoint와 연동하여 사용하기 위한 변수

int full_simulation =0;
int golden_simulator = 0;
int fault_simulator = 0;

pthread_t a_thread;
void *thread_result;
int res;
        
int restart_signal = 0;
int next_signal =0;

int sock;
struct sockaddr_in serv_addr;
char message[30];
int str_len;
int num;

char w_data[100];
char r_data[100];

int enroll_nv=0;			// 0이면 이름 , 1이면 값 
int enroll_nx; 			// 각 이름의 키워드 숫자 

char IP_str[20];
char PORT_str[10];


typedef struct Monitoring_Info
{
	int injection_time;
	int monitor_time;
	int num_of_output;
	char injection_module[100];
	char output_list[50][100];
	vpiHandle injection_module_h;
	vpiHandle output_h_list[50];

}MI;

MI monitor_info;

typedef struct Sending_Packet
{
	int ID; 		// 1 golden, 2 fault
	int type; 		// 1 control, 2 data
	char w_data_1[100];	// signal name or contorl
	char w_data_2[100]; 	// signal value

}SP;

SP packet;

int check_time = 1000000;
int start_signal = 0;

// /////////////////////////////////////////////////////////////////////////////////////////////////
// Fault Injection & Load State을 위한 변수
// ///////////////////////////////////////////////////////////////////////////////////////////////// 

char end_code[20];

int Snap_load_set = 0;
// Fault configuration structure
int fault_inject_set =0;
int number_of_fault =0;
int total_bit_weith = 0;
int Snap_image_load_num;		// 호출 번호 Snap shot-> 결함주입 시간이 결정되면 결함주입 시점 최근이전의 Snap shot을 로드.

FILE* hist_file;		// fault injection or masking 여부를 출력 -> ESRA에서 참고자료로 사용된다.
FILE* Loading_Point;

FILE* trace;
// 결함주입위치의 모듈정보를 모니터에 전달하기 위한 viphandle
vpiHandle	fault_module;

// 결함 모델의 정보를 정의하게될 구체조 fault_config 
typedef struct fault_config
{
	char design_top_model[50];				// 시뮬레이션 모델의 top module -> 모델 구조 탐색에 사용된다.
	char fault_location_name[100];			// 결함주입 위치의 이름
	int fault_location_type;				// 1: random 2: deterministic
	int fault_time_range_left;				// 결함주입 가능구간 / 시작점
	int fault_time_range_right;				// 결함주입 가능구간 / 끝점
	int fault_value;						// 1: stuck-at-0, 2: stuck-at-1, 3: stuck-at-x, 4: stuck-at-z
	int fault_latent;						// 결함의 유지시간
	int fault_type; 						// 1: transient, 0: permanent;
	
	int fault_range;						// single bit fault or multi bit(2~) fault
	
	int fault_injection_time;				// 결함주입 시간
	int fault_location_random_seed;			// 
	int fault_location_bit;					// 결함 주입 비트. (ex: 32bit bus 에서 7bit에 결함)
	
	//s_setval_value value_s;	
	int fault_location_vector_size;				// 오류주입 대상 위치의 bit 폭 
	char fault_location_incorrect_value[150];	// 오류주입 전의 데이터 값 
	
	vpiHandle fault_handle;						// 결함주입 위치의 vpi 핸들 값

}FC;


FC fault_configuration_list[10];			// 결함모델은 모두 10개를 선언 할 수있다. 이는 한 시뮬레이션에서 최대 10개의 결함을 동시에 주입 할수 있음을 의미한다.
int fault_list_current_num =0;				// fault_configuration_list 배열의 첨자 지시값
int fault_list_total_num =0;				// fault_configuration_list 선언된 결함모델 총 개수
int fault_injection_enable =0;				// 0: disable, 1: enable --> 결함주입 여부를 확인할 수 있다.


// 결함주입할때 사용활 VPI 함수의 전달 인자값 들 

struct t_cb_data 	cb_fault_enable[10];
struct t_cb_data 	cb_fault_release[10];

vpiHandle 		f_e_h[10];
vpiHandle 		f_r_h[10];

vpiHandle fault_loc_handle;
s_vpi_time 		time_fault_enable[10];
s_vpi_time 		time_fault_release[10];


// 임시 문자열 변수
char str[50];


// 문자열 버퍼를 초기화 하는 함수
void init_str()
{
	int i;
	for(i=0; i< 50; i++)
	{
		str[i] = 0;
	
	}

}


int total_fault_boundary =0;		// ??
int total_bit_width =0;
///////////////////////////////////////////////////////////////////////////////
// 2011 9 7 --> Partial Fault Injection 구현 
/*
	Partial Fault Injection은 계층구조로 설계한 verilog 모델에서 특정 계층의 
	모듈(이하 모듈 포함) 만을 선별적으로 선택하여 결함을 주입할 수 있음.
	고장 감내가 적용된 일부 모듈의 검증을 위해 사용 할 수 있음
*/

int fault_location_type =0;				// 0 normal, 1 partial module 
char partial_module_name[100];			// 결함을 주입할 모듈의 이름
int partial_set = 0;					// 0 false     1 true
int partial_boundary =0;				//	
int fault_boundary =0;					// ??

///////////////////////////////////////////////////////////////////////////////
// 2011 11 28 --> Multi-bit fault injection 구현
int fault_range =0;



int enroll_v=0;			// 0이면 이름 , 1이면 값 
int enroll_x; 			// 각 이름의 키워드 숫자 

void fault_config_enroll()
{
	int tem_con_num;


	if(enroll_v == 0)
	{	
		if(!strcmp(str,"fault_injection"))
		{
		
			enroll_x =1;
		}
		else if(!strcmp(str,"fault_num"))
		{
		
			enroll_x =2;
		}
		else if(!strcmp(str,"con"))
		{
		
			enroll_x =3;
		}
		else if(!strcmp(str,"fault_model"))
		{
		
			enroll_x =4;
		}
		else if(!strcmp(str,"fault_location"))
		{
		
			enroll_x =5;
		}
		else if(!strcmp(str,"fault_time_left"))
		{
		
			enroll_x =6;
		}
		else if(!strcmp(str,"fault_time_right"))
		{
		
			enroll_x =7;
		}
		else if(!strcmp(str,"fault_value"))
		{
		
			enroll_x =8;
		}	
		else if(!strcmp(str,"fault_latant"))
		{
		
			enroll_x =9;
		}	
		else if(!strcmp(str,"fault_type"))
		{
		
			enroll_x =10;
		}
		else if(!strcmp(str,"fault_boundary"))
		{
			enroll_x =11;
		}
		else if(!strcmp(str,"fault_range"))
		{
			enroll_x =12;
		}
		else if(!strcmp(str,"load_state"))
		{
			enroll_x =13;
		}
		else if(!strcmp(str,"end_code"))
		{
			enroll_x =14;
		}
		else if(!strcmp(str, "snapshot_interval"))
		{
			enroll_x =15;
		}
		else if(str[0] == '=')
		{
			enroll_v = 1;
		}
		else
		{
		
			enroll_x =0;
		}
		
	}
	else if(enroll_v == 1)
	{
		if(enroll_x == 1)				// 결함주입 여부를 설정 yes 는 결함주입, no 는 일반 시뮬레이션
		{
			if(!strcmp(str,"yes"))
			{
				fault_injection_enable =1;
			}
			else if(!strcmp(str,"no"))
			{
				fault_injection_enable =0;
			}
			else
			{
				vpi_printf("unkown configuration value : 1\n");			
			}
		
			enroll_v = 0;		// 초기화
		}
		else if(enroll_x == 2)				// 한 시뮬레이션에서 주입할 결함의 전체 갯수설정(일반적으로 1)
		{
			fault_list_total_num = atoi(str);
			
			enroll_v = 0;		
		}
		else if(enroll_x == 3)				// 만약 결함주입 개수가 여러개일 경우 index 지시자 값
		{
			tem_con_num = atoi(str);
			fault_list_current_num = tem_con_num-1;	// list 배열은 0부터 시작하기 대문에
					
			enroll_v = 0;		
		}		
		else if(enroll_x == 4)				// 시뮬레이션 모델의 top module 이름 등록
		{
			strcpy(fault_configuration_list[fault_list_current_num].design_top_model, str);
											
			enroll_v = 0;		
		}		
		else if(enroll_x == 5)
		{
			if(!strcmp(str,"none"))			// none으로 설정하면,시뮬레이션 모듈 내부의 모든 객체을 대상으로 결함을 주입 할 수 있다. random
			{
				fault_configuration_list[fault_list_current_num].fault_location_type = 1;
				fault_location_type =1;
				strcpy(fault_configuration_list[fault_list_current_num].fault_location_name, str);
			}
			else
			{
											// 특정 모듈을 선택 하여 집중적인 결함주입 실험을 실시 할 수 있다. 
				fault_configuration_list[fault_list_current_num].fault_location_type = 2;  // 2 partial module
				// 2011 9 7
				fault_location_type = 2;
				strcpy(partial_module_name, str);		// partial fault injection을 수행할 결함주입 위치 이름
				//////////end /////////////
				strcpy(fault_configuration_list[fault_list_current_num].fault_location_name, str);
			}

			enroll_v = 0;		
		}		
		else if(enroll_x == 6)
		{	
											// 결함주입 가능구간의 시작점
			fault_configuration_list[fault_list_current_num].fault_time_range_left = atoi(str);
				
			enroll_v = 0;		
		}		
		else if(enroll_x == 7)
		{
											// 결함주입 가능구간의 끝점
			fault_configuration_list[fault_list_current_num].fault_time_range_right = atoi(str);
				
			enroll_v = 0;		
		}		
		else if(enroll_x == 8)
		{
											// 결함주입 값을 설정, 주입 할 수 있는 결함 값은 stuck-at-0(1,x, z) 가 있다.
			if(str[0] == '0')
			{
				fault_configuration_list[fault_list_current_num].fault_value = 1;
			}
			else if(str[0] == '1')
			{
				fault_configuration_list[fault_list_current_num].fault_value = 2;			
			}
			else if(str[0] == 'x')
			{
				fault_configuration_list[fault_list_current_num].fault_value = 3;			
			}
			else if(str[0] == 'z')
			{
				fault_configuration_list[fault_list_current_num].fault_value = 4;			
			}
			else
			{
				vpi_printf("unkown configuration value : fault value\n");	
			}
			
			enroll_v = 0;		
		}		
		else if(enroll_x == 9)
		{
										//주입된 결함의 유지시간(결함 해제시간은 = 결함주입시간+결함 유지시간)

			fault_configuration_list[fault_list_current_num].fault_latent = atoi(str);
		
			enroll_v = 0;		
		}		
		else if(enroll_x == 10)
		{
		
										// 결함 발생비도 transient, permanent(intermittent 은 미구현)

			if(!strcmp(str,"transient"))			// random fualt injection 
			{
				fault_configuration_list[fault_list_current_num].fault_type = 1;
			}
			else if(!strcmp(str,"permanent"))			// random fualt injection 
			{
				fault_configuration_list[fault_list_current_num].fault_type = 2;
			}
			else 
			{
				vpi_printf("unkown configuration value : fault type\n");	
			}
					
			enroll_v = 0;		
		}
		else if(enroll_x == 11)
		{
										// 결함주입 위치로 설정가능한 전체 오브젝트(net, reg) 갯수 -> random seed로 사용
			fault_boundary = atoi(str);
			enroll_v = 0;	
		
		}
		else if(enroll_x == 12)
		{
										// 주입된 결함의 유지 시간 설정

			fault_configuration_list[fault_list_current_num].fault_range = atoi(str);
			enroll_v = 0;
		}
		else if(enroll_x == 13)
		{
										
			 Snap_load_set = atoi(str);
			 enroll_v = 0;
		}
		else if(enroll_x == 14)
		{
			strcpy(end_code, str);								

			enroll_v = 0;
		}
		else if(enroll_x == 15)
		{	
			snapshot_interval = atoi(str);
			enroll_v =0;
		}
		else
		{
										// 해당사항이 없는 설정
			enroll_v = 0;		
		}
			
	}
	else
	{
	
	
	}
	
}


// configuration list 에 등록되어 있는 오류주입 설정 값을 확인 
void fault_configuration_check()
{
	int i;
	
	for(i = 0; i<fault_list_total_num ; i++)
	{
		vpi_printf("design_top_model		: %s\n", fault_configuration_list[i].design_top_model);
		vpi_printf("fault_location_name 		: %s\n", fault_configuration_list[i].fault_location_name);
		vpi_printf("fault_location_type 		: %d\n", fault_configuration_list[i].fault_location_type);
		vpi_printf("fault_time_range_left 	: %d\n", fault_configuration_list[i].fault_time_range_left);
		vpi_printf("fault_time_range_right 	: %d\n", fault_configuration_list[i].fault_time_range_right);
		vpi_printf("fault_value 			: %d\n", fault_configuration_list[i].fault_value);
		vpi_printf("fault_latent 			: %d\n", fault_configuration_list[i].fault_latent);
		vpi_printf("fault_type 			: %d\n", fault_configuration_list[i].fault_type);
		vpi_printf("fault_range 			: %d\n", fault_configuration_list[i].fault_range);
		vpi_printf("Snapshot Interval			: %d\n", snapshot_interval);
			
		vpi_printf(" \n*******************\n");
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// 결함주입 시간을 설정한는 함수 
void fault_injection_time_setting()
{
	int i, temp_time;
	
	for(i = 0; i<fault_list_total_num ; i++)		// 설정된 결함이 1개 이상일 수 있다.
	{
		if(fault_configuration_list[i].fault_time_range_left == fault_configuration_list[i].fault_time_range_right)
		{	
			// 결함주입 가능 시간의 시작과 끝이 같다면, deterministic 하게 결함주입 시간을 설정 할 수 있다.
			fault_configuration_list[i].fault_injection_time = fault_configuration_list[i].fault_time_range_left;
		}
		else
		{	// 시작시간과 종료시간이 다르면, 시간간격 사이에서 random 하게 결함주입 시간 값을 설정한다.
			temp_time = fault_configuration_list[i].fault_time_range_right - fault_configuration_list[i].fault_time_range_left;
			fault_configuration_list[i].fault_injection_time = ((fault_configuration_list[i].fault_time_range_left + (rand() % (temp_time/2)) *2 ))+1;
			//fault_configuration_list[i].fault_injection_time = ((fault_configuration_list[i].fault_time_range_left + (rand() % temp_time)));

		}
		
		vpi_printf("*** fault injection time => %d \n", fault_configuration_list[i].fault_injection_time);

	
		fprintf(trace, " ==>  fault injection time 	= %d 	\n", fault_configuration_list[i].fault_injection_time);
	}


	// 가속실험에 필요한 결함주입 시간 정보를 모니터로 전송한다. 
	// 현재는 1개의 결함에 대해서만 jumpping 이 가능
	 monitor_info.injection_time = 	fault_configuration_list[0].fault_injection_time;


}
///////////////////////////////////////////////////////////////////////////////////////////////////////
// 결함주입 위치를 설정하는 함수..

void fault_location_random_setting()
{
	int i;
		
	for(i = 0; i<fault_list_total_num ; i++)
	{
			// 전체 결함주입 가능 개수중 하나를 random 하게 설정한다.(번호로 갖고 이후에 모델 탐색중 해단 번호에 해당하는 object를 결함주입 위치로 설정
			fault_configuration_list[i].fault_location_random_seed = (rand() % fault_boundary)+1;	
	}
	
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// 결함주입 위치의 핸들 값을 획득 하는 함수



void fault_injection_check(vpiHandle h)
{
	int i;


//vpi_printf("%s -->  %d\n", vpi_get(vpiName, h), vpi_get(vpiSize, h));

//total_bit_weith += vpi_get(vpiSize, h);	

	if(fault_collapsing_set == 0)
	{

		for(i = 0; i<fault_list_total_num ; i++) 
		{
		
		
			if(fault_location_type == 2)
			{	
							// partial location 에서 결함주입 위치 핸들 획득
				if(partial_boundary == fault_configuration_list[i].fault_location_random_seed)
				{
					
					// reg [0:7] memory [3000000] 에 오류가 들어가는 문제를 방지..
					if(vpi_get(vpiSize, h) < 100 )	// 프로세서 모델에서 100bit 이상의 bus을 갖는 object가 없다고 가정한다.
					{
						fault_configuration_list[i].fault_handle =h;
//vpi_printf(" 111==> Fault location 	: %s \n", vpi_get_str(vpiFullName, h));	
						monitor_info.injection_module_h= fault_module;
					}
					else
					{
						partial_boundary--;
					
					}				
				
				
				}			
	
			}
			else if(fault_location_type == 1)
			{
							// full location 에서 결함주입 위치 핸들 획득
				if(total_fault_boundary == fault_configuration_list[i].fault_location_random_seed)
				{
					// reg [0:7] memory [3000000] 에 오류가 들어가는 문제를 방지..
					
					if(vpi_get(vpiSize, h) < 100 )	// 프로세서 모델에서 100bit 이상의 bus을 갖는 object가 없다고 가정한다.
					{
						fault_configuration_list[i].fault_handle =h;
//vpi_printf(" 222==> Fault location 	: %s \n", vpi_get_str(vpiFullName, h));							
				              monitor_info.injection_module_h = fault_module;
					}
					else
					{
						total_fault_boundary--;
					}				
				}		
			}
			else
			{
				// setting error
	
			}
			
		
		}
	}
	else if(fault_collapsing_set == 1)
	{
		
		char tmp_buff[150];
		strcpy(tmp_buff, vpi_get_str(vpiFullName, h));
		for(i = 0; i<fault_list_total_num ; i++) 
		{

			if(strcmp(FL_list.fault_location, tmp_buff) == 0)
			{
				fault_configuration_list[i].fault_handle =h;

//vpi_printf(" 333 ==> Fault location 	: %s \n", vpi_get_str(vpiFullName, h));

			//	vpi_printf("OK : %s\n",tmp_buff); 


				return;

			}
		}
		
	}
	else
	{


	}
}

///////////////////////////////////////////////////////////////////////////////////////////
// 시뮬레이션 모델 내에 net 형 데이터를 탐색하는 함수

void search_net(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;


	itr = vpi_iterate(vpiNet, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module), vpiPort
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
	
		printf("Port OBJ : %s --> %d \n", vpi_get_str(vpiFullName , find_handle), vpi_get(vpiSize, find_handle));
	
		if(partial_set == 1)
		{
			total_bit_width = total_bit_width + vpi_get(vpiSize, find_handle);
			partial_boundary++;
			fault_injection_check(find_handle);
		}
		else
		{
			total_fault_boundary++;
			fault_injection_check(find_handle);
		}
	}
	
}

///////////////////////////////////////////////////////////////////////////////////////////
// 시뮬레이션 모델 내에 reg 형 데이터를 탐색하는 함수

void search_reg(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;
	
	itr = vpi_iterate(vpiReg, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
//		printf("REG OBJ : %s --> %d \n", vpi_get_str(vpiFullName, find_handle), vpi_get(vpiSize, find_handle));

		if(partial_set == 1)
		{
			total_bit_width = total_bit_width + vpi_get(vpiSize , find_handle);
			partial_boundary++;
			fault_injection_check(find_handle);
		}
		else
		{
			total_fault_boundary++;
			fault_injection_check(find_handle);
		}
		
	}

}

///////////////////////////////////////////////////////////////////////////////////////////
// 시뮬레이션 모델 내에 module 를 탐색하는 함수

void search_module(vpiHandle root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;

			
	itr = vpi_iterate(vpiModule, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	//		vpi_printf("------------>  Test 1 \n");
	//		vpi_printf(" ----------->  Test 2	: %s \n", vpi_get_str(vpiFullName, root_handle));

	
		//	itr = vpi_iterate(vpiModule, NULL);	// vpiReg(reg), vpiNet(wire), vpiModule(module)

	if(itr == NULL)		// 최 하위 모듈 -- 더이상의 하위 계층의 모듈은 존재하지 않는다.
	{
	        if(strcmp(partial_module_name, vpi_get_str(vpiFullName, root_handle)) == 0)
        	{
					// partial location fault injection 에서 해당 모듈이 검색 되었다.	

			fault_module = root_handle;	
        
	        	partial_set = 1;
	                search_net(root_handle);
        	        search_reg(root_handle);
			partial_set = 0;
	        }
		return;
	}
	// 부분적인 결함주입을 하기위한 조건으로..
	// 선택되어진 모듈과 그 하위 모듈에 대한 결함 범위를 잡을 수 있도록 해야 한다.


	
	if(strcmp(partial_module_name, vpi_get_str(vpiFullName, root_handle)) == 0)
	{
		// partial location fault injection 에서 해당 모듈이 검색 되었다. -- 선택된 모듈의 하위 모듈이 존재한다.
		partial_set = 1;
		fault_module = root_handle;	

		search_net(root_handle);
		search_reg(root_handle);
	
	}
		
	while( (find_handle = vpi_scan(itr)))
	{
		// 현 module 내부의 net형, reg형 데이터를 탐색한다.
		fault_module = find_handle;	

		search_net(find_handle);
		search_reg(find_handle);
		
		// 현 module 내부의 하위 계층의 module을 탐색한다.
		search_module(find_handle);
	}
	
	if(strcmp(partial_module_name, vpi_get_str(vpiFullName, root_handle)) == 0)
	{
		partial_set = 0;
	
	}	
}
////////////////////////////////////////////////////////////////////////////////////
// vpi getvalue 함수를 사용하기 위한 전달인자 구조체 변수.... 
    	static s_vpi_value value_s = {vpiBinStrVal};
	static p_vpi_value value_p = &value_s;
	
	static s_vpi_value f_value_s = {vpiBinStrVal};
	static p_vpi_value f_value_p = &f_value_s;
	
	static s_vpi_time time_s = {vpiSimTime};
	static p_vpi_time time_p = &time_s;

////////////////////////////////////////////////////////////////////////////////////
// 결함 주입 callback 함수

int fault_enable(p_cb_data cb_data_p)
{

	vpi_printf(" -------> Fault injection \n");
	
	number_of_fault++;
	
	//fault_loc_handle 이 핸들에 결함 주입..
//	char g_value[100];
	
	int *xx;
	int x;
	char _fault =0;
	
	xx = (int *)cb_data_p->user_data;
	x= (int)xx;

	
	// 결함값 설정	
	if(fault_configuration_list[x].fault_value == 1)
	{
		_fault = '0';
	}
	else if(fault_configuration_list[x].fault_value == 2)
	{
		_fault = '1';
	}
	else if(fault_configuration_list[x].fault_value == 3)
	{
		_fault = 'x';
	}
	else if(fault_configuration_list[x].fault_value == 4)
	{
		_fault = 'z';
	}
	else
	{
		
	}	

	//vpi_printf("AAAAA------>        %d \n", x);
	
	// 결함주입 위치의 현재 값 획득
	
	vpi_get_value(fault_configuration_list[x].fault_handle, value_p);
	vpi_get_value(fault_configuration_list[x].fault_handle, f_value_p);

	vpi_printf(" ==>  original-value 	= %s 	\n", value_p->value.str);
	fprintf(trace, " ==>  ori-value 	= %s 	\n", value_p->value.str);
	
//	strcpy(g_value, value_p->value.str);


	// 결함 값을 만든다.
	if(fault_configuration_list[x].fault_location_bit > -1)
	{

	
		// 현재 값 backup
		strcpy(fault_configuration_list[x].fault_location_incorrect_value, value_p->value.str );

		// multi-bit fault 에서 fault range가 결함주입 위치의 bit vector(bus) 보다 클 경우 모든 bit에 결함값을 주입한다.
		if(fault_configuration_list[x].fault_location_vector_size <= fault_configuration_list[x].fault_range)// data weith < fault range
		{
			// all fault bit
			int k;
			for(k=0; k < fault_configuration_list[x].fault_location_vector_size ; k++)
				fault_configuration_list[x].fault_location_incorrect_value[k] = _fault;
					
		}
		else
		{	

			// 1bit fault 결함주입. fault_location_bit 는 random 하게 설정, 이하 최대 5bit 까지 multi bit을 주입 할 수 있음
			if(fault_configuration_list[x].fault_range == 1)
			{
				fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit] = _fault;//value_s.value.str[0];

			}
			else if(fault_configuration_list[x].fault_range == 2)
			{
				// multi-bit 의 진행방향을 설정한다. ( ex: fault location bit 가 1일 경우, case 1=> 1, 0 bit 주입, case  => 1, 2 bit 주입)
				// 여기서는 내림 차순으로 결함을 주입한다.
				if(fault_configuration_list[x].fault_location_bit >0)
				{
					// down
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-1] = _fault;
				
				}
				else
				{
					// up
				
				}
			
			}
			else if(fault_configuration_list[x].fault_range == 3)
			{
				if(fault_configuration_list[x].fault_location_bit >1)
				{
					// down
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-1] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-2] = _fault;
				
				}
				else
				{
					// up
				
				}
			}
			else if(fault_configuration_list[x].fault_range == 4)
			{
				if(fault_configuration_list[x].fault_location_bit >2)
				{
					// down
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-1] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-2] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-3] = _fault;
				
				}
				else
				{
					// up
				
				}
			}
			else if(fault_configuration_list[x].fault_range == 5)
			{
				if(fault_configuration_list[x].fault_location_bit >4)
				{
					// down
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-1] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-2] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-3] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit-4] = _fault;
				
				}
				else
				{
					// up
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit+1] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit+2] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit+3] = _fault;
					fault_configuration_list[x].fault_location_incorrect_value[fault_configuration_list[x].fault_location_bit+4] = _fault;
				
				}
			}
			else
			{
			
			
			}
		}

		// 생성된 결함 값을 VPI 구조체 변수에 할당.
		strcpy(f_value_p->value.str, fault_configuration_list[x].fault_location_incorrect_value);
	

/*		
		////////////////////////////////////
		if((strcmp(g_value, f_value_p->value.str )) == 0)
		{
		    // 결함주입 전과 후의 값이 같으면 더이상의 시뮬레이션은 의미가 없다.

			
			//시뮬레이션 정지..
			if(ICARUS ==1)
			{
				vpi_control(vpiFinish);  // ICARUS
			}
			else if(Modelsim == 1)
			{
				//vpi_printf("reset simulation \n");
				//vpi_control(vpiStop);  // Modelsim
			}
			else
			{
				// 다른 시뮬레이터
			}

vpi_printf(" ==>  8 !!!");

		}	
		else
		{
			fault_inject_set = 1;
		}
		////////////////////////////////////		
*/		
	}
	else
	{
		// 선택된 신호의 bit vector값이 1bit 값일 경우. multi-bit 설정 정보는 무신된다.
		// 1bit 결함값을 생성한다.	
		strcpy(fault_configuration_list[x].fault_location_incorrect_value, value_p->value.str );
		fault_configuration_list[x].fault_location_incorrect_value[0] = _fault;//value_s.value.str[0];
		strcpy(f_value_p->value.str, fault_configuration_list[x].fault_location_incorrect_value);
		
/*		
		if(g_value[0] == f_value_p->value.str[0] )
		{
		// 두 값이 같으면 더이상의 시뮬레이션은 의미가 없다.
			

			//시뮬레이션 정지..
			if(ICARUS ==1)
			{
				vpi_control(vpiFinish);  // ICARUS
			}
			else if(Modelsim == 1)
			{
				//vpi_printf("reset simulation \n");
				//vpi_control(vpiStop);  // Modelsim
			}
			else
			{
				// 다른 시뮬레이터
			}


		}
		else
		{
			fault_inject_set = 1;
		}
*/
	
	}


	// 결함 값을 시뮬레이션 오브젝트(net, reg)에 할당한다.
//	vpi_put_value(fault_configuration_list[x].fault_handle, f_value_p, time_p ,vpiForceFlag);
	vpi_put_value(fault_configuration_list[x].fault_handle, f_value_p, NULL ,vpiNoDelay);

	vpi_printf(" ==>  fault-value 	= %s \n", f_value_p->value.str);
	fprintf(trace, "==>  fault-value 	= %s \n", f_value_p->value.str);
	
	if(number_of_fault == fault_list_total_num )
	{
		if(fault_inject_set == 1)
		{
			fputs("injection\n", hist_file);
			vpi_printf(" Fault Injection OK\n");
			fprintf(trace, "Fault Inject \n");
		}
		else
		{
			fputs("masking\n", hist_file);
			
			vpi_printf(" Fault Masking OK \n");
			fprintf(trace, "Fault Masking \n");
		}
		
		fclose(hist_file);
		
	}
	
	fclose(trace);
	
	
	return 0;

}

///////////////////////////////////////////////////////////////////////////////////
// 결함 주입을 멈추는 callback 함수

int fault_release(p_cb_data cb_data_p)
{

	vpi_printf(" -------> Fault release \n\n");
	
	int *xx;
	int x;
	xx = (int *)cb_data_p->user_data;
	
	x= (int)xx;
	
	//fault_loc_handle 이 핸들에 결함 해제
	
	//vpi_get_value(fault_configuration_list[x].fault_handle, value_p);
	
	//vpi_put_value(fault_configuration_list[x].fault_handle,f_value_p, time_p ,vpiReleaseFlag);
	vpi_put_value(fault_configuration_list[x].fault_handle, value_p, NULL ,vpiReleaseFlag);
	
	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
// 결함주입 system task main callback 함수,,

static PLI_INT32 ICARUS_FI_calltf(char *data)
{
      vpi_printf("\n\n ICARUS Fault Injection, from SET & SEU version.\n");
      
      hist_file = fopen("Snap_hist_file", "at");
      trace = fopen("Injection-log", "at");
      FILE *tmp = fopen("Injection-log.tmp", "rt");
       
      char *logtmp;
      logtmp = (char *)malloc(sizeof(char) * 5); 
      fgets(logtmp, 5, tmp);
      int number = atoi(logtmp);
      
      fclose(tmp);
      
      FILE *tmp1 = fopen("Injection-log.tmp", "wt");
      
      if(number == 99)
      {
      	fprintf(tmp1, "0");
      }
      else
      {
       	fprintf(tmp1, "%d", number+1);
      }
      
      fclose(tmp1);
      
      fprintf(trace, "\n\n =============  %d ============== \n", number);
            
      // fault configuration data setting..
     	int state;

	int str_num =0;
	char ch;
		
	// 파일의 개방
	FILE* f = fopen("fault_configuration", "rt");
	if(f == NULL) {
	
		vpi_printf("non fualt injection ! \n");
		return 0;
	}
	
	init_str();
	
	while(1)
	{
		ch = fgetc(f);
		
		
		
		if(ch == ' ')// || ch == '\n')
		{
			str_num =0;
			//io_printf("%s\n", str);
			fault_config_enroll();
			init_str();
			// 문자열 비교 및 인식 값 확인 절차 수행 
			
			//io_printf("space\n");
		}
		else if(ch == '\n')
		{
			str_num =0;
			//io_printf("%s\n", str);
			fault_config_enroll();
			init_str();
			//io_printf("new line\n");
			// 문자열 비교 및 인식 값 확인 절차 수행 
		}
		else 
		{
			str[str_num++] = ch;
			//io_printf("%c\n", ch);
		}
		
		if(feof(f) != 0)
			break;
			
	}
	
	
	// 파일의 종결
	state = fclose(f);
	if(state != 0) {
	
		vpi_printf("fault configuration file close error ! \n");
		return 0;
	} 
      
	fault_configuration_check();
	////////////////////////////////////////////////////////////////////////////////
      	// 임위적 오류주입을 가능하게 하기 위한 rand 함수 초기화 
	//srand(time(NULL));
	struct timespec tp;
	int rs;
	rs = clock_gettime(CLOCK_REALTIME, &tp);
	srand(tp.tv_nsec);
      
	
	// fault injection Time setting
	fault_injection_time_setting();
	
	// fault injection location setting
	fault_location_random_setting();
      
      // 모델 탐색 및 오류주입 위치 핸들 수신
	vpiHandle top_module;
	
	top_module= vpi_handle_by_name(fault_configuration_list[0].design_top_model, NULL);
	
vpi_printf(" ----------->  TopModule	: %s \n", vpi_get_str(vpiFullName, top_module));

	// 현재 부분 결함주입과 크기 제한옵션을 구현되어 있지 않음(9월 24일)
	
	search_net(top_module);
	search_reg(top_module);
	search_module(top_module);
	
	
	if(fault_location_type == 2)
	{
		vpi_printf("*** partial_fault_boundary => %d :: %d \n", partial_boundary, total_bit_width);
		
	}
	else
	{
		vpi_printf("*** total_fault_boundary => %d \n", total_fault_boundary);
		
	}      
     
      
      
      
      // 오류주입 백터 위치 결정 [0:31] 중 4 or 10 or 25 or etc
      //vpi_printf(" of size %d \n", vpi_get(vpiSize, fault_loc_handle));
	
      	int i;
	
	for(i = 0; i<fault_list_total_num ; i++) 
	{

// 오류주입위치 알림	 vpiFullName, vpiName
vpi_printf(" ==> Fault location 	: %s \n", vpi_get_str(vpiFullName, fault_configuration_list[i].fault_handle));
fprintf(trace, " ==> Fault location 	: %s \n", vpi_get_str(vpiFullName, fault_configuration_list[i].fault_handle));
		fault_configuration_list[i].fault_location_vector_size = vpi_get(vpiSize, fault_configuration_list[i].fault_handle);
		
		
		
		if(fault_configuration_list[i].fault_location_vector_size >1)
		{
			
			
			fault_configuration_list[i].fault_location_bit = (rand() % fault_configuration_list[i].fault_location_vector_size);
			
			
		}
		else
		{
			fault_configuration_list[i].fault_location_bit = -1;		// why? -1
		}	
	
		//vpi_printf(" of size %d  중  %d bit \n", vpi_get(vpiSize, fault_loc_handle), fault_configuration_list[i].fault_location_bit);
	
	}
            		
            // time callback 함수 등록.... 
	    int j;
		
		int k;
	    
	if(Snap_load_set == 1)
	{
		for(j=0; j <= fault_list_total_num; j++)
		{
		
			for(k=1; k <100; k++)
			{



				if((fault_configuration_list[j].fault_injection_time >= (k*snapshot_interval)) && (fault_configuration_list[j].fault_injection_time < ((k+1)*snapshot_interval)) )
				{
					fault_configuration_list[j].fault_injection_time -= (k * snapshot_interval);
					Snap_image_load_num =k;
				
					break;
				}

			}	
		
		}
	}		    
   
	if(fault_injection_enable ==1)
	{
		for(i = 0; i<fault_list_total_num ; i++) 
		{
			time_fault_enable[i].high 		= 0;
			time_fault_enable[i].low			= fault_configuration_list[i].fault_injection_time;
			time_fault_enable[i].type 		= vpiSimTime;
			
			cb_fault_enable[i].reason		= cbReadWriteSynch;
			cb_fault_enable[i].cb_rtn		= fault_enable;
			cb_fault_enable[i].time 		= &time_fault_enable[i];
			cb_fault_enable[i].obj			= f_e_h[i];
			cb_fault_enable[i].value 		= 0;
			cb_fault_enable[i].user_data		= (PLI_BYTE8 *)(i);

                        vpi_register_cb(&cb_fault_enable[i]);


			
			////////////////////////////////////////////////////////////////////////		
			//	fault_configuration_list[fault_list_current_num].fault_type = 1;
			if(fault_configuration_list[i].fault_type == 1 )
			{
				time_fault_release[i].high 		= 0;
				time_fault_release[i].low		= fault_configuration_list[i].fault_injection_time+fault_configuration_list[i].fault_latent;
				time_fault_release[i].type 		= vpiSimTime;
			
				cb_fault_release[i].reason		= cbReadWriteSynch;
				cb_fault_release[i].cb_rtn		= fault_release;
				cb_fault_release[i].time 		= &time_fault_release[i];
				cb_fault_release[i].obj			= f_r_h[i];
				cb_fault_release[i].value 		= 0;
				cb_fault_release[i].user_data		= (PLI_BYTE8 *)(i);	
	
				vpi_register_cb(&cb_fault_release[i]);
			}
		
		}
	
	}
	
      
      
      
      
      
      // main loop 종료
      
      return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Snap Load
//
////////////////////////////////////////////////////////////////////////////////////////////////////////


FILE* snap_load_DB; 

struct t_cb_data 	snap_load_callback;		// 차후에 구조체가 될 수 있음
vpiHandle 		s_load_h;
s_vpi_time 		snap_load_enable;		// snap shot save time
vpiHandle top_load_module;

static s_vpi_value value_load_s = {vpiBinStrVal};
static p_vpi_value value_load_p = &value_load_s;
static s_vpi_time time_load_s = {vpiSimTime};
static p_vpi_time time_load_p = &time_load_s;

char str_load[100];

// 문자열 버퍼를 초기화 하는 함수
void init_load_str()
{
	int i;
	for(i=0; i< 100; i++)
	{
		str_load[i] = 0;
	
	}

}

int C_Queue[160];
int C_Q_header = 0;
int C_Q_tailed = 0;
int file_end =0;
char pri_str[100];

void C_Queue_Fill()
{

	char ch;
	int q_tmp_buff[8];


	while( 8 < 160-(C_Q_header-C_Q_tailed) )
	{


		if(feof(snap_load_DB) != 0)
		{
			file_end = 1;
			return;	
		}

		ch = fgetc(snap_load_DB);
	
		int x;
		for(x =7; x>=0 ; x--)
		{

			C_Queue[(C_Q_header) % 160] = (int)( (ch >> x) & 0x01);
			C_Q_header++;
		}

	}


}

FILE* load_debug;
void str_load_search()
{
	init_load_str();
	int str_num =0;
	char ch;
	
	while(1)
	{
		ch = fgetc(snap_load_DB);
		
		if(ch == ' ')// || ch == '\n')
		{

		}
		else if(ch == '\n')
		{
			str_num =0;
			return; 
		}
		else 
		{
			str_load[str_num++] = ch;
		}
		
		if(feof(snap_load_DB) != 0)
			break;
			
	}

}

void search_load_net(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;
	
	itr = vpi_iterate(vpiNet, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		str_load_search();
		vpi_get_value(find_handle, value_load_p);
		strcpy(value_load_p->value.str, str_load );
		vpi_put_value(find_handle, value_load_p, time_load_p ,vpiNoDelay);	
	}
	
}
void search_load_MemoryWord(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;

	//vpiRegBit, vpiMemory, vpiMemoryWord
	itr = vpi_iterate(vpiMemoryWord, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		str_load_search();
		vpi_get_value(find_handle, value_load_p);
		strcpy(value_load_p->value.str, str_load);
		vpi_put_value(find_handle, value_load_p, time_load_p ,vpiNoDelay);		
	}


}

void search_load_Memory(vpiHandle  root_handle)
{

	vpiHandle itr;
	vpiHandle find_handle;

	//vpiRegBit, vpiMemory, vpiMemoryWord
	itr = vpi_iterate(vpiMemory, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		search_load_MemoryWord(find_handle);
	}
}

void search_load_reg(vpiHandle  root_handle)
{
	vpiHandle itr;
	vpiHandle find_handle;
	
	itr = vpi_iterate(vpiReg, root_handle);	// vpiReg(reg), vpiNet(wire), vpiModule(module)
	
	if(itr == NULL)
		return;
		
	while( (find_handle = vpi_scan(itr)))
	{
		str_load_search();
		vpi_get_value(find_handle, value_load_p);
		strcpy(value_load_p->value.str, str_load);
		vpi_put_value(find_handle, value_load_p, time_load_p ,vpiNoDelay);
	}
}


void search_load_module(vpiHandle root_handle)
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
		search_load_net(find_handle);
		search_load_reg(find_handle);
		search_load_Memory(find_handle);
		search_load_module(find_handle);
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

int snap_load(p_cb_data cb_data_p)
{

	vpi_printf(" -------> Snap load enable time \n\n");
	
//	struct timespec tp_ls;
//	int rs_ls;
//	rs_ls = clock_gettime(CLOCK_REALTIME, &tp_ls);
//	printf("Loading Start : [%ld.%ld] \n", tp_ls.tv_sec, tp_ls.tv_nsec);

	search_load_net(top_load_module);
	search_load_reg(top_load_module);
	search_load_Memory(top_load_module);
	search_load_module(top_load_module);
	fclose(snap_load_DB);
	
//	struct timespec tp_le;
//	int rs_le;
//	rs_le = clock_gettime(CLOCK_REALTIME, &tp_le);
//	printf("check time : [%ld.%ld] \n", tp_le.tv_sec, tp_le.tv_nsec);

	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 snap_load_calltf(char *data)
{

/*
	while(1)
	{
		if(Masking_check == 1)
		{
			break;
		}
	}
*/

      vpi_printf("\n\n Snap Load, from VPI.  %d\n", Snap_image_load_num);

      char Point[10];
      sprintf(Point, "%d\n", Snap_image_load_num); 

      Loading_Point = fopen("Loading_Point", "at");
      fputs(Point, Loading_Point);
      fclose(Loading_Point);

      	
      
//      if(fault_injection_enable == 0)			// 임시제거 .. 복원 해야 함
//      {
//      	return 0;
//      }
	//결함주입 시간을 확인하여 test vector 인가 값 조정
	

	char buff[100];      

	if(Snap_image_load_num == 0)
	{
		vpi_printf("\n Snap_image load Error 0 \n");		
		
		return;
	}



      
	sprintf(buff, "./Snap_DB_%d", Snap_image_load_num * snapshot_interval);


/*
load_debug = fopen("./load_debug", "a");
fprintf(load_debug, "%d\n", snap_load_DB);
fclose(load_debug);
*/
     
    
      // 모델 탐색 및 오류주입 위치 핸들 수신
//if(Snap_image_load_num >= 3)
//{
	snap_load_DB = fopen(buff, "rt");


	top_load_module= vpi_handle_by_name("tb_74181_b", NULL);
	
      
  
	snap_load_enable.high 		= 0;
	snap_load_enable.low			= 100;			// snap shot time
	snap_load_enable.type 		= vpiSimTime;
	
	snap_load_callback.reason		= cbAfterDelay;
	snap_load_callback.cb_rtn		= snap_load;
	snap_load_callback.time 		= &snap_load_enable;
	snap_load_callback.obj			= s_load_h;
	snap_load_callback.value 		= 0;
	snap_load_callback.user_data		= (PLI_BYTE8 *)1;
			
	////////////////////////////////////////////////////////////////////////		
	vpi_register_cb(&snap_load_callback);
//}	
		
      
      return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 golden_sim_calltf(char *data)
{
        vpi_printf("\n\n Golden Sim, from VPI.\n");

	
	network_config();
	golden_simulator=1;

	packet.ID = 1;



	packet.type = 1;
	strcpy(packet.w_data_1, "golden");
      	write(sock, &packet, sizeof(packet));
      	vpi_printf("WRITE\n");
        // processing....



	res = pthread_create(&a_thread, NULL, receiving_thread, NULL);

	if(res != 0)
	{
		vpi_printf("thread error\n");
	}



	vpi_printf("CLOSE\n");

	while(1)
	{
		if(start_signal == 1)
		{	
			if(full_simulation == 0)
			{
      				stop_simulation(monitor_info.monitor_time);
			}
			break;
		}
	}

      return 0;
}



static PLI_INT32 fault_sim_calltf(char *data)
{
        vpi_printf("\n\n fault Sim, from VPI.\n");
	
	network_config();
	//Masking_check = 1;	//결함주입 시뮬레이션 쪽은 load checkpoint 지점을 이미 알고 있으니까..

	fault_simulator = 1;

	packet.ID = 2;


	packet.type = 1;
	strcpy(packet.w_data_1, "fault");
      	write(sock,&packet, sizeof(packet));

        // processing....
	// 결함주입 루틴에서 선택된 결함주입 시간과 결함주입 모듈을 모니터에 전송한다.
	char buf[12];
	packet.type = 1;
	strcpy(packet.w_data_1, "fault_time");
	sprintf(buf, "%d", fault_configuration_list[0].fault_injection_time);
	strcpy(packet.w_data_2, buf);
	write(sock, &packet, sizeof(packet));

	packet.type = 1;
	strcpy(packet.w_data_1, "fault_location");
	strcpy(packet.w_data_2, vpi_get_str(vpiFullName, monitor_info.injection_module_h));
	write(sock, &packet, sizeof(packet));

	packet.type = 1;
	strcpy(packet.w_data_1, "Checkpoint");
	sprintf(buf, "%d", Snap_image_load_num);
	strcpy(packet.w_data_2, buf);
	write(sock, &packet, sizeof(packet));


	res = pthread_create(&a_thread, NULL, receiving_thread, NULL);

	if(res != 0)
	{
		vpi_printf("thread error\n");
	}

	vpi_printf("CLOSE\n");


	while(1)
	{
		if(start_signal == 1)
		{	
			if(full_simulation == 0)
			{
      				stop_simulation(monitor_info.monitor_time);
			}
			break;
		}

	}

      
      return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void *receiving_thread(void *arg)
{

	vpi_printf("Thread Start \n");

        while(1)
        {
                        read(sock, r_data, sizeof(r_data));

                        if(strcmp(r_data, "Hello") == 0)
                        {	
				// 연결접속 확인
                                vpi_printf(" Hello \n");
                        }
			else if(strcmp(r_data, "start") == 0)
			{
				start_signal = 1;

			}
			else if(strcmp(r_data, "full_simulation") == 0)
			{
				full_simulation = 1;

			}
			else if(strcmp(r_data, "restart") == 0)
			{
				restart_signal = 1;
			}
			else if(strcmp(r_data, "next") == 0)
			{
				next_signal = 1;
			}
			else if(strcmp(r_data, "Checkpoint_jumping") == 0)
			{
				char buff[12];

				read(sock, r_data, sizeof(r_data));	// checkpoint loadding 위치
				strcpy(buff, r_data);
				Snap_image_load_num = atoi(buff);		


			}
			else if(strcmp(r_data, "fault_info") == 0)		
			{
				// golden simulator에서 결함주입 시험 정보를 획득한다.				

				read(sock, r_data, sizeof(r_data));	//결함주입 모듈 정보획득
				strcpy(monitor_info.injection_module, r_data);
					


				read(sock, r_data, sizeof(r_data));	// 결함주입 시간 정보 획득
				char buff[12];
				strcpy(buff, r_data);

				int fault_time = atoi(buff);		// 결함주입 시간
				monitor_info.monitor_time = atoi(buff);	
				monitor_info.monitor_time += 100;

			}
			else if(strcmp(r_data, "monitoring_output") == 0)
			{
				// golden/fault simulator에서 monitoring output 을 획득하기 위한 output full name 획득
				monitor_info.num_of_output =0;

				
				while(1)
				{
					read(sock, r_data, sizeof(r_data));
					
					if(strcmp(r_data, "monitoring_output_end") == 0)
					{

						break;
					}

					strcpy(monitor_info.output_list[monitor_info.num_of_output], r_data);
					monitor_info.num_of_output++;

				}
				// 수집한 output port의 handle을 조회한다.
				int y;
				for(y=0; y < monitor_info.num_of_output; y++)
				{
					monitor_info.output_h_list[y] = vpi_handle_by_name(monitor_info.output_list[y] , NULL);
		

				}	



			}			
			else if(strcmp(r_data, "masking") == 0)
			{
				// masking이 확정되면 더이상 시뮬레이션 할 필요가 없기 때문에
				// 소켓 연결을 종료하고 시뮬레이션을 정지(미작업)  시킨다.
				vpi_printf("\n \n masking \n ");
							


				static s_vpi_value END_s = {vpiBinStrVal};
				static p_vpi_value END_p = &END_s;
				static s_vpi_time time_END_s = {vpiSimTime};
				static p_vpi_time time_END_p = &time_END_s;
				vpiHandle END_h;		
				END_h = vpi_handle_by_name("or1200_testbench.iwb_adr_o", NULL);
				vpi_get_value(END_h, END_p);


				// 강제 종료 코드. target depandancy
				char buffer[100];
				strcpy(buffer, end_code );		// x2~x5  100011111011100 // FFT_edit 100011111110000

				strcpy(END_p->value.str, buffer);
				

				vpi_put_value(END_h, END_p, time_END_p ,vpiNoDelay);
				
				restart_signal = 1;	//임시..


				break;
			}	
        }

        close(sock);



}

// VPI함수만으로는 시뮬레이터를 일시중지, 다시수행 기능을 처리 할 수 없다.
// 이를 위해 일시 중지는 callback 함수의 while 문, 다시수행은 Monitor의 명령 
// 으로 기능을 구현하고자한다.

// stop_simulation 함수는 시간을 전달 인자로 받고, 전달받은 시간이 되면 
// callback -> while loop로 시뮬레이션이 일시 중지 된다.


struct t_cb_data 	stop_callback;		// 차후에 구조체가 될 수 있음
vpiHandle 		stop_h;
s_vpi_time 		stop_enable;		// snap shot save time


vpiHandle		Monitor_S[5];

int stop_function(p_cb_data cb_data_p)
{


//모니터에게 현재 Simulator가 설정된 시간에 대기하고 있다는 정보를 Monitor에게 알린다.

	if(golden_simulator == 1)
	{
	      	packet.type = 1;
		strcpy(packet.w_data_1, "golden_wait");
		write(sock, &packet, sizeof(packet));
	}
	else if(fault_simulator == 1)
	{

	      	packet.type = 1;
		strcpy(packet.w_data_1, "fault_wait");
		write(sock, &packet, sizeof(packet));


	}
	else
	{

	}

// 정지 상태에서 Simulator의 특정 포트의 상태를 Monitor에게 전송 
// Monitor가 두 시뮬레이터의 상태를 비교할 수 있도록 한다.




	static s_vpi_value value_s_m = {vpiBinStrVal};
	static p_vpi_value value_p_m = &value_s_m;
	

	int i;
	for(i=0; i< monitor_info.num_of_output; i++)
	{
	     	packet.type = 2;
		vpi_get_value(monitor_info.output_h_list[i], value_p_m);
		strcpy(packet.w_data_1, vpi_get_str(vpiFullName, monitor_info.output_h_list[i]));
		strcpy(packet.w_data_2, value_p_m->value.str);
		write(sock, &packet, sizeof(packet));
	}

     	packet.type = 1;
	strcpy(packet.w_data_1, "time_stemp");
	write(sock, &packet, sizeof(packet));



	vpi_printf("**********ALL Send***********\n");

// 상태 전송후 비교 결과에 따라 "시뮬레이션 중지" 또는 "다음 Step 이동" 또는 "계속 진행"의
// 수행 명령을 받는다. --> 현재(2012 7 9)는 계속 진행만 구현

	int deadlock_count =0;
	int deadlock_ready =0;

	while(1)
	{
		deadlock_count++;
		
		if(restart_signal == 1)
		{
			restart_signal = 0;
			vpi_printf("restart simulation \n");
			break;
		}
		else if(next_signal == 1)
		{
			next_signal = 0;
			check_time = 100;
			stop_simulation(check_time);
			break;
		}
		else
		{

		}



		if(deadlock_count > 500000000)
		{
	     		packet.type = 1;
			strcpy(packet.w_data_1, "Dead_Lock");
			write(sock, &packet, sizeof(packet));
		
			vpi_printf("Dead Lock Detection \n");
			break;

		}
		else if(deadlock_count > 100000000 && deadlock_ready == 0)
		{
			vpi_printf("Dead Lock Ready \n");
			deadlock_ready = 1;
		}
		else
		{

		}

	}	

}


void stop_simulation(int time)
{
	stop_enable.high 		= 0;
	stop_enable.low			= time;			// snap shot time
	stop_enable.type 		= vpiSimTime;
	
	stop_callback.reason		= cbAfterDelay;
	stop_callback.cb_rtn		= stop_function;
	stop_callback.time 		= &stop_enable;
	stop_callback.obj			= stop_h;
	stop_callback.value 		= 0;
	stop_callback.user_data		= (PLI_BYTE8 *)1;
			
	////////////////////////////////////////////////////////////////////////		
	vpi_register_cb(&stop_callback);
	
}

// 임시 문자열 변수
char netstr[50];


// 문자열 버퍼를 초기화 하는 함수
void init_netstr()
{
	int i;
	for(i=0; i< 50; i++)
	{
		netstr[i] = 0;
	
	}

}


char IP_str[20];
char PORT_str[10];

int network_config()
{
	vpi_printf("Network Configuation !!!\n");

     	int state;

	int str_num =0;
	char ch;
		
	// 파일의 개방
	FILE* f = fopen("network_configuration", "rt");
	if(f == NULL) {
	
		vpi_printf("non network configuration file !! \n");
		return 0;
	}
	
	init_netstr();
	
	while(1)
	{
		ch = fgetc(f);
		
		
		
		if(ch == ' ')// || ch == '\n')
		{
			str_num =0;
			//io_printf("%s\n", str);
			net_config_enroll();
			init_netstr();
			// 문자열 비교 및 인식 값 확인 절차 수행 
			
			//io_printf("space\n");
		}
		else if(ch == '\n')
		{
			str_num =0;
			//io_printf("%s\n", str);
			net_config_enroll();
			init_netstr();
			//io_printf("new line\n");
			// 문자열 비교 및 인식 값 확인 절차 수행 
		}
		else 
		{
			netstr[str_num++] = ch;
			//io_printf("%c\n", ch);
		}
		
		if(feof(f) != 0)
			break;
			
	}
	
	
	// 파일의 종결
	state = fclose(f);
	if(state != 0) {
	
		vpi_printf("fault configuration file close error ! \n");
		return 0;
	}





// Socket Setting
        sock = socket(AF_INET, SOCK_STREAM, 0);

          memset(&serv_addr, 0, sizeof(serv_addr));
          serv_addr.sin_family=AF_INET;
          serv_addr.sin_addr.s_addr=inet_addr(IP_str);
          serv_addr.sin_port=htons(atoi(PORT_str));


// Server Connection
        if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
	    vpi_printf("Connection Error\n");

}

void net_config_enroll()
{
	int tem_con_num;


	if(enroll_nv == 0)
	{	
		if(!strcmp(netstr,"ServerIP"))
		{
		
			enroll_nx =1;
		}
		else if(!strcmp(netstr,"Port"))
		{
		
			enroll_nx =2;
		}
		else if(netstr[0] == '=')
		{
			enroll_nv = 1;
		}
		else
		{
		
			enroll_nx =0;
		}
		
	}
	else if(enroll_nv == 1)
	{
		if(enroll_nx == 1)				// 서버 IP 
		{
			
			strcpy(IP_str, netstr);
			vpi_printf("%s\n", IP_str);

			enroll_nv = 0;		// 초기화
		}
		else if(enroll_nx == 2)				// PORT
		{
			strcpy(PORT_str, netstr);
			vpi_printf("%s\n", PORT_str);		

			enroll_nv = 0;		
		}
		else
		{
										// 해당사항이 없는 설정
			enroll_nv = 0;		
		}
			
	}
	else
	{
	
	
	}
	
}


static PLI_INT32 fault_collapsing_calltf(char *data)
{

	vpi_printf("fault collapsing !!! \n");
	
	fault_collapsing_set = 1;
	FL_list.list_count =0;

	char ch;
	char obj_buffer[150];
	int obj_count =0;
	int state;


	FILE* f = fopen("fault_location_list", "rt");
	
	if(f == 0)
	{
		vpi_printf("Don't reading fault_location_list\n");
	}


	while(1)
	{
		ch = fgetc(f);
		
		
		
		if(ch == '\n')
		{
		//	vpi_printf("%s\n", obj_buffer);

			strcpy(FL_list.list[FL_list.list_count], obj_buffer);
			FL_list.list_count++;


			obj_count =0;
			int k;
			for(k=0; k<150; k++)
			{
				obj_buffer[k] =0;				

			}

		}
		else 
		{
			obj_buffer[obj_count++] = ch;
		}
		
		if(feof(f) != 0)
			break;
			
	}
	
	// fault location selecting

	struct timespec tp;
	int rs;
	rs = clock_gettime(CLOCK_REALTIME, &tp);
	srand(tp.tv_nsec);

	int fault_point = rand() % FL_list.list_count;
	strcpy(FL_list.fault_location, FL_list.list[fault_point]);


	// certification
/*
	int k;
	for(k=0; k < FL_list.list_count; k++)
	{

		vpi_printf("%d : %s\n",k+1 , FL_list.list[k]);

	}
*/
//	vpi_printf("\n%d : %s\n", fault_point, FL_list.fault_location);

	
	// 파일의 종결
	state = fclose(f);
	if(state != 0) {
	
		vpi_printf("fault configuration file close error ! \n");
		return 0;
	}


}

///////////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 time_check_calltf(char *data)
{
	struct timespec tp;
	int rs;
	rs = clock_gettime(CLOCK_REALTIME, &tp);
	printf("check time : [%ld.%ld] \n", tp.tv_sec, tp.tv_nsec);

	return 0;

}


////////////////////////////////////////////////////////////////////////////////////////

void golden_sim_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$golden_sim";
      tf_data.calltf    = golden_sim_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}

////////////////////////////////////////////////////////////////////////////////////////

void fault_sim_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$fault_sim";
      tf_data.calltf    = fault_sim_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

void snap_load_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$snap_load";
      tf_data.calltf    = snap_load_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

void ICARUS_FI_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$ICARUS_FI";
      tf_data.calltf    = ICARUS_FI_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void fault_collapsing_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$fault_collapsing";
      tf_data.calltf    = fault_collapsing_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}



/////////////////////////////////////////////////////////////////////////////////////////////////
void time_check_register()
{
      s_vpi_systf_data tf_data;

      tf_data.type      = vpiSysTask;
      tf_data.tfname    = "$time_check";
      tf_data.calltf    = time_check_calltf;
      tf_data.compiletf = 0;
      tf_data.sizetf    = 0;
      vpi_register_systf(&tf_data);

}

void (*vlog_startup_routines[100])() =
{
  ICARUS_FI_register,
//  snap_save_register,
  snap_load_register,
  golden_sim_register,
  fault_sim_register,
  fault_collapsing_register,
  time_check_register,
//  ACE_MAP_register,
  0 /*** final entry must be 0 ***/
};

