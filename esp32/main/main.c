#include "nvs_flash.h" 
#include "gps.h" 

void app_main(void) 
{
     nvs_flash_init(); gps_init(); 
}
