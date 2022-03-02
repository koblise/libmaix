#include "stdio.h"
#include <stdint.h>
#include <stdbool.h>

#include "global_config.h"
#include "global_build_info_time.h"
#include "global_build_info_version.h"

#include "libmaix_cam.h"
#include "libmaix_disp.h"
#include "libmaix_image.h"
#include "libmaix_nn.h"
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <signal.h>

#include "libmaix_nn_decoder_retinaface.h"
#include "main.h"
#include "time_utils.h"

#define LOAD_IMAGE 0

#if LOAD_IMAGE
    #define SAVE_NETOUT 1
#endif

static volatile bool program_exit = false;


int loadFromBin(const char* binPath, int size, signed char* buffer)
{
    FILE* fp = fopen(binPath, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "fopen %s failed\n", binPath);
        return -1;
    }
    int nread = fread(buffer, 1, size, fp);
    if (nread != size)
    {
        fprintf(stderr, "fread bin failed %d\n", nread);
        return -1;
    }
    fclose(fp);

    return 0;
}

int save_bin(const char* path, int size, uint8_t* buffer)
{
    FILE* fp = fopen(path, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "fopen %s failed\n", path);
        return -1;
    }
    int nwrite = fwrite(buffer, 1, size, fp);
    if (nwrite != size)
    {
        fprintf(stderr, "fwrite bin failed %d\n", nwrite);
        return -1;
    }
    fclose(fp);

    return 0;
}


void nn_test(struct libmaix_disp* disp)
{

    printf("--image module init\n");
    libmaix_image_module_init();
    libmaix_nn_module_init();
    libmaix_camera_module_init();


    #ifdef CONFIG_ARCH_V831
     int res_w = 224 , res_h = 224;
     #endif
     #ifdef CONFIG_ARCH_R329
     int res_w = 320, res_h = 320;
     #endif
    int input_w = res_w, input_h = res_h;
    int disp_w = 240, disp_h = 240;
    libmaix_nn_t* nn = NULL;
    libmaix_err_t err = LIBMAIX_ERR_NONE;
    libmaix_nn_decoder_t* decoder = NULL;
    libmaix_nn_decoder_retinaface_result_t result;


    printf("--cam create\n");

   
    libmaix_image_t* img = libmaix_image_create(res_w, res_h, LIBMAIX_IMAGE_MODE_RGB888, LIBMAIX_IMAGE_LAYOUT_HWC, NULL, true);
    libmaix_image_t * show  =  libmaix_image_create(disp->width, disp->height, LIBMAIX_IMAGE_MODE_RGB888, LIBMAIX_IMAGE_LAYOUT_HWC, NULL, true);

    //  libmaix_image_t* img  = NULL;
    //  libmaix_image_t* show = NULL;


    // libmaix_cam_t* cam = libmaix_cam_create(0, 320, 320, 1, 1);
    libmaix_cam_t* cam = libmaix_cam_create(0, res_w, res_h, 1, 0);

    #ifdef CONFIG_ARCH_V831
    libmaix_cam_t* cam2 = libmaix_cam_create(1, disp_w, disp_h, 0, 0);
    #endif

    if(!cam)
    {
        printf("create cam fail\n");
    }
    printf("--cam start capture\n");
    err = cam->start_capture(cam);
    #ifdef CONFIG_ARCH_V831
    err = cam2->start_capture(cam2);
    #endif
    if(err != LIBMAIX_ERR_NONE)
    {
        printf("start capture fail: %s\n", libmaix_get_err_msg(err));
        goto end;
    }

    printf("--init\n");
    libmaix_nn_model_path_t model_path = {

        #ifdef CONFIG_ARCH_V831
        .awnn.param_path = "/root/Run_model/models/Retinaface.param",
        .awnn.bin_path = "/root/Run_model/models/Retinaface.bin",
        #endif
        // R329
        #ifdef CONFIG_ARCH_R329
         .aipu.model_path = "/root/models/aipu_Retinaface_320.bin",
        #endif
    };
    
        #ifdef CONFIG_ARCH_R329
        int min_sizes_list [] = {10, 16, 24, 32, 48, 64, 96, 128, 192, 256};
        int steps_list []  = {8, 16, 32, 64};
        #endif

         #ifdef CONFIG_ARCH_V831
        int steps_list []= {8, 16, 32};
        int min_sizes_list []= {16, 32, 64,128,256,512};
        #endif

    libmaix_nn_decoder_retinaface_config_t config = {
        .nms = 0.2,
        .score_thresh = 0.6,
        .input_w = input_w,
        .input_h = input_h,

        //R329
        // .variance = {0.1, 0.2},
        // .min_sizes_len = 10,   //  new membership
        // .steps_len = 4,  //  new membership
        // .steps = &steps_list,
        // .min_sizes = &min_sizes_list,


        //V831
        .variance ={0.1,0.2},

        #ifdef CONFIG_ARCH_V831
        .steps = &steps_list,
        .min_sizes = &min_sizes_list,
        .min_sizes_len = 6,   //  new membership
        .steps_len = 3,  //  new membership
        #endif

        #ifdef CONFIG_ARCH_R329
        .steps = &steps_list,
        .min_sizes = &min_sizes_list,
        .min_sizes_len = 10,   //  new membership
        .steps_len = 4,  //  new membership
        #endif
        
    };
    libmaix_nn_layer_t input = {
        .w = input_w,
        .h = input_h,
        .c = 3,
        .dtype = LIBMAIX_NN_DTYPE_INT8,
        .data = NULL,
        .need_quantization = true,
        .buff_quantization = NULL
    };
    printf("-- init decoder\n");
    decoder = libmaix_nn_decoder_retinaface_create();
    err = decoder->init(decoder, &config);
    if(err != LIBMAIX_ERR_NONE)
    {
        printf("start capture fail: %s\n", libmaix_get_err_msg(err));
        goto end;
    }
    printf("-- channel num: %d\n", config.channel_num);
    libmaix_nn_layer_t out_fmap[3] = {
        {
            .w = 4,
            .h = 1,
            .c = config.channel_num,
            .dtype = LIBMAIX_NN_DTYPE_FLOAT,
            .data = NULL,
            .layout = LIBMAIX_NN_LAYOUT_CHW,
        },
        {
            .w = 2,
            .h = 1,
            .c = config.channel_num,
            .dtype = LIBMAIX_NN_DTYPE_FLOAT,
            .data = NULL,
            .layout = LIBMAIX_NN_LAYOUT_CHW,
        },
        {
            .w = 10,
            .h = 1,
            .c = config.channel_num,
            .dtype = LIBMAIX_NN_DTYPE_FLOAT,
            .data = NULL,
            .layout = LIBMAIX_NN_LAYOUT_CHW,
        }
    };
    char* inputs_names[] = {"input0"};
    char* outputs_names[] = {"output0", "output1", "output2"};
    
    #ifdef CONFIG_ARCH_V831
    libmaix_nn_opt_param_t opt_param = {
        .awnn.input_names             = inputs_names,
        .awnn.output_names            = outputs_names,
        // .awnn.input_ids               = NULL,
        // .awnn.output_ids              = NULL,
        .awnn.encrypt                 = false,
        .awnn.input_num               = 1,              // len(input_names)
        .awnn.output_num              = 3,              // len(output_names)
        .awnn.mean                    = {127.5, 127.5, 127.5},
        .awnn.norm                    = {0.0078125, 0.0078125, 0.0078125},
    };
    #endif

    #ifdef CONFIG_ARCH_R329
    libmaix_nn_opt_param_t opt_param = {
        .aipu.input_names             = inputs_names,
        .aipu.output_names            = outputs_names,
        .aipu.input_num               = 1,              // len(input_names)
        .aipu.output_num              = 3,              // len(output_names)
        .aipu.mean                    = {104, 117, 123},
        .aipu.norm                    = {1, 1, 1},
        .aipu.scale                   = {32.752407 , 29.865177 , 14.620169},    //Only R329 has this option (r0p0 SDK)
    };
    # endif


    float* output_buffer = (float*)malloc(out_fmap[0].c * out_fmap[0].w * out_fmap[0].h * sizeof(float));
    if(!output_buffer)
    {
        printf("no memory!!!\n");
        goto end;
    }
    out_fmap[0].data = output_buffer;
    float* output_buffer2 = (float*)malloc(out_fmap[1].c * out_fmap[1].w * out_fmap[1].h * sizeof(float));
    if(!output_buffer2)
    {
        printf("no memory!!!\n");
        goto end;
    }
    out_fmap[1].data = output_buffer2;
    float* output_buffer3 = (float*)malloc(out_fmap[2].c * out_fmap[2].w * out_fmap[2].h * sizeof(float));
    if(!output_buffer3)
    {
        printf("no memory!!!\n");
        goto end;
    }
    out_fmap[2].data = output_buffer3;
    
    int8_t* quantize_buffer = (int8_t*)malloc(input.w * input.h * input.c);
    if(!quantize_buffer)
    {
        printf("no memory!!!\n");
        goto end;
    }
    input.buff_quantization = quantize_buffer;
    printf("-- nn create\n");
    nn = libmaix_nn_create();
    if(!nn)
    {
        printf("libmaix_nn object create fail\n");
        goto end;
    }
    printf("-- nn object init\n");
    err = nn->init(nn);
    if(err != LIBMAIX_ERR_NONE)
    {
        printf("libmaix_nn init fail: %s\n", libmaix_get_err_msg(err));
        goto end;
    }
    printf("-- nn object load model\n");
    err = nn->load(nn, &model_path, &opt_param);
    if(err != LIBMAIX_ERR_NONE)
    {
        printf("libmaix_nn load fail: %s\n", libmaix_get_err_msg(err));
        goto end;
    }
    printf("-- start loop\n");
    while(!program_exit)
    {
#if LOAD_IMAGE
        printf("-- load input bin file\n");
        loadFromBin("/root/test_input/input_256x448.bin", res_w * res_h * 3, rgb_img->data);
#else
        err = cam->capture_image(cam, &img);
        # ifdef CONFIG_ARCH_V831
        err = cam2->capture_image(cam2, &show);
        #endif
        
        if(err != LIBMAIX_ERR_NONE)
        {
            // not ready， sleep to release CPU
            if(err == LIBMAIX_ERR_NOT_READY)
            {
                usleep(20 * 1000);
                continue;
            }
            else
            {
                printf("capture fail: %s\n", libmaix_get_err_msg(err));
                break;
            }
        }
#endif

        // forward process
        input.data = (uint8_t *)img->data;

        CALC_TIME_START();
        err = nn->forward(nn, &input, out_fmap);
        CALC_TIME_END("forward");
        if(err != LIBMAIX_ERR_NONE)
        {
            printf("libmaix_nn forward fail: %s\n", libmaix_get_err_msg(err));
            goto end;
        }

// #if SAVE_NETOUT

//         save_bin("loc.bin", out_fmap[0].w * out_fmap[0].h * out_fmap[0].c * sizeof(float), out_fmap[0].data);
//         save_bin("conf.bin", out_fmap[1].w * out_fmap[1].h * out_fmap[1].c * sizeof(float), out_fmap[1].data);
//         save_bin("landmark.bin", out_fmap[2].w * out_fmap[2].h * out_fmap[2].c * sizeof(float), out_fmap[2].data);

// #endif

        CALC_TIME_START();
        decoder->decode(decoder,out_fmap, &result);
        CALC_TIME_END("decode face info");
        libmaix_image_color_t color = {
            .rgb888.r = 255,
            .rgb888.g = 0,
            .rgb888.b = 0
        };
        // // draw image 
        #ifdef CONFIG_ARCH_R329
        for(int i=0; i < result.num; ++i)
        {
            if(result.faces[i].score > config.score_thresh)
            {
                int x1 = result.faces[i].box.x * img->width;
                int y1 = result.faces[i].box.y * img->height;
                int x2 = x1 + result.faces[i].box.w * img->width;
                int y2 = y1 + result.faces[i].box.h * img->height;
                
                libmaix_cv_image_draw_rectangle(img, x1, y1, x2, y2, MaixColor(255,0,0),2);

                printf("x1:%d , x2;%d \n",x1,y1);

                for(int j=0; j<5; ++j)
                {
                    int x = result.faces[i].points[j * 2] * img->width;
                    int y = result.faces[i].points[j * 2 + 1] * img->height;
                    libmaix_cv_image_draw_rectangle(img,x-2,y-2,x+2,y+2,MaixColor(0,255,23) , -1);
                    printf("x:%d , y:%d\n ",x,y);
                
                }
            }
        }
        err = libmaix_cv_image_resize(img, disp->width, disp->height, &show);
        disp->draw_image(disp, show);
        #endif


        #ifdef CONFIG_ARCH_V831
        for(int i=0; i < result.num; ++i)
        {
            if(result.faces[i].score > config.score_thresh)
            {
                int x1 = result.faces[i].box.x * show->width;
    //     printf("--image destory\n");
    //     libmaix_image_destroy(&img);
    // }

                printf("x1:%d , x2;%d \n",x1,y1);

                for(int j=0; j<5; ++j)
                {
                    int x = result.faces[i].points[j * 2] * show->width;
                    int y = result.faces[i].points[j * 2 + 1] * show->height;
                    libmaix_cv_image_draw_rectangle(show,x-2,y-2,x+2,y+2,MaixColor(0,255,23) , -1);
                    printf("x:%d , y:%d\n ",x,y);
                
                }
            }
        }
        disp->draw_image(disp, show);
        #endif



#if LOAD_IMAGE
        break;
#endif
    }
end:
    if(output_buffer)
    {
        free(output_buffer);
    }
    if(output_buffer2)
    {
        free(output_buffer2);
    }
    if(output_buffer3)
    {
        free(output_buffer3);
    }
    if(nn)
    {
        libmaix_nn_destroy(&nn);
    }
    // if(img)
    // {
    //     printf("--image destory\n");
    //     libmaix_image_destroy(&img);
    // }
    if(cam)
    {
         printf("--cam destory\n");
         libmaix_cam_destroy(&cam);
    }
        
    if(decoder)
    {
        decoder->deinit(decoder);
        libmaix_nn_decoder_destroy(&decoder);
    }
    printf("--image module deinit\n");
    libmaix_nn_module_deinit();
    libmaix_image_module_deinit();
}

static void handle_signal(int signo)
{
  if (SIGINT == signo || SIGTSTP == signo || SIGTERM == signo || SIGQUIT == signo || SIGPIPE == signo || SIGKILL == signo)
  {
    program_exit = true;
  }
}

int main(int argc, char* argv[])
{
    struct libmaix_disp* disp = libmaix_disp_create(0);
    if(disp == NULL) {
        printf("creat disp object fail\n");
        return -1;
    }

    

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("program start\n");
    nn_test(disp);
    printf("program end\n");

    libmaix_disp_destroy(&disp);
    return 0;
}
