#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

#include <malloc.h>

/**
 * srTCM
 */
static struct rte_meter_srtcm* meters[4];

int
qos_meter_init(void)
{
    // init meters
    int i;
    for(i = 0; i < 4; i++){
        struct rte_meter_srtcm* meter = (struct rte_meter_srtcm *)
            malloc(sizeof(struct rte_meter_srtcm));
        struct rte_meter_srtcm_params* param = (struct rte_meter_srtcm_params*)
            malloc(sizeof(struct rte_meter_srtcm_params));
        param->cir = 1000000 * 46;
        param->cbs = 2048;
        param->ebs = 2048;

        rte_meter_srtcm_config(meter, param);
        
        meters[i] = meter;
    }
    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    // get flow's meter
    struct rte_meter_srtcm* meter = meters[flow_id];

    // get color
    return rte_meter_srtcm_color_blind_check(meter, time, pkt_len);
}


/**
 * WRED
 */

static struct rte_red_config * red_cfg_R[4];   // red
static struct rte_red_config * red_cfg_Y[4];   // yellow
static struct rte_red_config * red_cfg_G[4];   // green

static struct rte_red * red;

static unsigned q;

int
qos_dropper_init(void)
{
    red = (struct rte_red *)malloc(sizeof(struct rte_red));
    int i;
    for(i = 0; i < 4; i++){
        red_cfg_G[i] = (struct rte_red_config *)malloc(sizeof(struct rte_red_config));
        red_cfg_Y[i] = (struct rte_red_config *)malloc(sizeof(struct rte_red_config));
        red_cfg_R[i] = (struct rte_red_config *)malloc(sizeof(struct rte_red_config));
    }
    

    // init run time data
    rte_red_rt_data_init(red);

    // read configuration from file
    /*
    int wq_log2;
    int min_th;
    int max_th;
    int maxp_inv;
    char buf[255];
    
    FILE * fp = fopen("../config","r");
    printf("\n\n");
    for(i = 0; i < 4; i++){ 
        fgets(buf, 255, fp);
        printf(buf);
        fscanf(fp, "%d%d%d%d\n", &wq_log2, &min_th, &max_th, &maxp_inv);
        printf("%d, %d, %d, %d\n", wq_log2, min_th, max_th, maxp_inv);
        rte_red_config_init(red_cfg_G[i], wq_log2, min_th, max_th, maxp_inv);
        
        fscanf(fp, "%d%d%d%d\n", &wq_log2, &min_th, &max_th, &maxp_inv);
        printf("%d, %d, %d, %d\n", wq_log2, min_th, max_th, maxp_inv);
        rte_red_config_init(red_cfg_Y[i], wq_log2, min_th, max_th, maxp_inv);
        
        fscanf(fp, "%d%d%d%d\n", &wq_log2, &min_th, &max_th, &maxp_inv);
        printf("%d, %d, %d, %d\n", wq_log2, min_th, max_th, maxp_inv);
        rte_red_config_init(red_cfg_R[i], wq_log2, min_th, max_th, maxp_inv);
    }
    printf("\n\n");
    */
   rte_red_config_init(red_cfg_G[0],10, 28, 256, 10);
   rte_red_config_init(red_cfg_Y[0],10, 128, 512, 10);
   rte_red_config_init(red_cfg_R[0],12, 512, 1023, 10);
   
   rte_red_config_init(red_cfg_G[1],10, 28, 256, 10);
   rte_red_config_init(red_cfg_Y[1],11, 128, 512, 10);
   rte_red_config_init(red_cfg_R[1],12, 128, 520, 10);

   rte_red_config_init(red_cfg_G[2],9, 28, 256, 10);
   rte_red_config_init(red_cfg_Y[2],9, 128, 512, 10);
   rte_red_config_init(red_cfg_R[2],11, 100, 500, 10);

   rte_red_config_init(red_cfg_G[3],9, 28, 256, 10);
   rte_red_config_init(red_cfg_Y[3],9, 100, 430, 10);
   rte_red_config_init(red_cfg_R[3],10, 100, 430, 10);

    
    q = 0;
    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    int result = 0;
    if(color == RED){
        result = rte_red_enqueue(red_cfg_R[flow_id], red, q, time);
    }else if(color == YELLOW){
        result = rte_red_enqueue(red_cfg_R[flow_id], red, q, time);
    }else{
        result =  rte_red_enqueue(red_cfg_R[flow_id], red, q, time);
    }
    if(result == 0){
        q ++;   // measured in packets
        return 0;
    }else{
        return 1;
    }
}