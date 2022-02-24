#include "stdio.h"
#include <stdint.h>
#include <stdbool.h>

#include "global_config.h"
#include "global_build_info_time.h"
#include "global_build_info_version.h"

#include "libmaix_cam.h"
#include "libmaix_disp.h"
#include "libmaix_image.h"
#include "libmaix_cv_image.h"
#include "libmaix_nn.h"
#include "libmaix_nn_decoder_yolo2.h"
#include "main.h"
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

/*a struct to config the carmera display and image settings*/
typedef struct
{
    struct libmaix_disp* disp;
    libmaix_image_t*       img;
}callback_param_t;


//get model from a binary file
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
/*use opencv to draw object box */
void on_draw_box(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t class_id, float prob, char* label, void* arg)
{
    callback_param_t* data = (callback_param_t*)arg;
    struct libmaix_disp* disp = data ? data->disp : NULL;
    libmaix_image_t* img = data ? data->img : NULL;
    char* default_label = "unknown";
    char temp[50];
    // libmaix_err_t err;
    static uint32_t last_id = 0xffffffff;   //set a window to quit the programe
    if(id != last_id)
    {
        printf("----image:%d----\n", id);
    }
    if(label)
    {
        printf(", label:%s\n", label);
    }
    else
    {
        label = default_label;
        printf("\n");
    }
    libmaix_image_color_t color = {
        .rgb888.r = 255,
        .rgb888.g = 255,
        .rgb888.b = 255
    };
    libmaix_image_color_t color2 = {
        .rgb888.r = 255,
        .rgb888.g = 0,
        .rgb888.b = 0
    };
    if(disp && img)
    {
        snprintf(temp, sizeof(temp), "%s:%.2f", label, prob);
        // img->draw_rectangle(img, x, y, w, h, color, false, 4);
        // img->draw_string(img, temp, x+4, y+4, 16, color2, NULL);

        //     libmaix_cv_image_draw_string()
        // libmaix_cv_image_draw_rectangle(img, 10, 10, 130, 120, MaixColor(255, 0, 0), 2);
        // libmaix_cv_image_draw_line(img, 10, 10, 130, 120, MaixColor(255, 0, 0), 2);
        // libmaix_cv_image_draw_string(img, 0, 120, "test123[]-=", 1.0, MaixColor(255, 0, 255), 2)
        
        
        int x1 = x ;
        int x2 = x + w;
        int y1 = y ;
        int y2 = y + h;
        libmaix_cv_image_draw_string(img,x1,y2,label,1.2, MaixColor(255, 0, 255), 2);
        libmaix_cv_image_draw_rectangle(img, x1, y1, x2, y2, MaixColor(255,0,0),2);

    
    }
    last_id = id;
}


/*use voc dataset to predict the position and lable of it*/
void nn_test(struct libmaix_disp* disp)
{
#if TEST_IMAGE
    int ret = 0;
#endif
    int count = 0;
    struct libmaix_cam* cam = NULL;
    libmaix_image_t* img;
    libmaix_image_t* show;
    callback_param_t callback_arg;

    libmaix_nn_t* nn = NULL;
    libmaix_err_t err = LIBMAIX_ERR_NONE;
    libmaix_err_t err0 = LIBMAIX_ERR_NONE;

    uint32_t res_w = 224, res_h = 224;

    char* labels[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "mouse", "microbit", "ruler", "cat", "peer", "ship", "apple", "car", "pan", "dog", "umbrella", "airplane", "clock", "grape", "cup", "left", "right", "front", "stop", "back"};
    int class_num = 35;
    float anchors [10] =  {2.44, 2.25, 5.03, 4.91, 3.5, 3.53, 4.16, 3.94, 2.97, 2.84};

    // char* labels[] = {"aeroplane","bicycle","bird","boat","bottle","bus","car","cat","chair","cow","diningtable","dog","horse","motorbike","person","pottedplant","sheep","sofa","train","tvmonitor"};
    // int class_num = 20;
    // float anchors[10] = {0.4165, 0.693 , 0.9765, 1.6065, 1.5855, 3.122 , 2.821 , 1.8515,3.612 , 3.7275};


    uint8_t anchor_len = sizeof(anchors) / sizeof(float) / 2; //five anchors

    libmaix_nn_decoder_yolo2_config_t yolo2_config = {
        .classes_num     = class_num,
        .threshold       = 0.5,   //Confidence level
        .nms_value       = 0.4,
        .anchors_num     = 5,
        .anchors         = anchors,
        .net_in_width    = 224,
        .net_in_height   = 224,
        .net_out_width   = 7,
        .net_out_height  = 7,
        .input_width     = res_w,
        .input_height    = res_w
    };
    libmaix_nn_decoder_t* yolo2_decoder = NULL;

#define TEST_IMAGE   0
#define DISPLAY_TIME 1

#if DISPLAY_TIME
    struct timeval start, end;
    int64_t interval_s;
    #define CALC_TIME_START() do{gettimeofday( &start, NULL );}while(0)
    #define CALC_TIME_END(name)   do{gettimeofday( &end, NULL ); \
                                interval_s  =(int64_t)(end.tv_sec - start.tv_sec)*1000000ll; \
                                printf("%s use time: %lld us\n", name, interval_s + end.tv_usec - start.tv_usec);\
            }while(0)
#else
    #define CALC_TIME_START() 
    #define CALC_TIME_END(name) 
#endif

    printf("--nn module init\n");
    libmaix_nn_module_init();
    printf("--image module init\n");
    libmaix_image_module_init();
    libmaix_camera_module_init();

    /*create the net process img is not display image*/
    printf("--create image\n");
    img = libmaix_image_create(res_w, res_h, LIBMAIX_IMAGE_MODE_RGB888, LIBMAIX_IMAGE_LAYOUT_HWC, NULL, true);
    // use LIBMAIX_IMAGE_MODE_RGB888 for read RGB88 image from FS test
    // for camera LIBMAIX_IMAGE_MODE_YUV420SP_NV21 is enough
    if(!img)
    {
        printf("create RGB image fail\n");
        goto end;
    }
    // the show image for display
    show = libmaix_image_create(disp->width, disp->height, LIBMAIX_IMAGE_MODE_RGB888, LIBMAIX_IMAGE_LAYOUT_HWC, NULL, true);
    if(!show)
    {
        printf("create RGB image fail\n");
        goto end;
    }


    // camera init
#if !TEST_IMAGE
    printf("--create cam\n");
    cam = libmaix_cam_create(0, res_w, res_h, 1, 1);
    if(!cam)
    {
        printf("create cam fail\n");
        goto end;
    }
    printf("--cam start capture\n");
    err = cam->start_capture(cam);
    if(err != LIBMAIX_ERR_NONE)
    {
        printf("start capture fail: %s\n", libmaix_get_err_msg(err));
        goto end;
    }
#endif
    printf("--yolo init\n");
    libmaix_nn_model_path_t model_path = {
        // .normal.model_path = "./model/aipu_yolo_VOC2007.bin",
        .normal.model_path = "/root/models/aipu_onnx_cards_224_35.bin",
    };
    libmaix_nn_layer_t input = {
        .w = yolo2_config.net_in_width,
        .h = yolo2_config.net_in_height,
        .c = 3,
        .dtype = LIBMAIX_NN_DTYPE_UINT8,
        .data = NULL,
        .need_quantization = true,
        .buff_quantization = NULL
    };
    libmaix_nn_layer_t out_fmap = {
        .w = yolo2_config.net_out_width,
        .h = yolo2_config.net_out_height,
        .c = (class_num + 5) * anchor_len,
        .dtype = LIBMAIX_NN_DTYPE_FLOAT,
        .data = NULL
    };
    char* inputs_names[] = {"input0"};
    char* outputs_names[] = {"output0"};
    // float Scale[] = {10.872787};
    libmaix_nn_opt_param_t opt_param = {
        .normal.input_names             = inputs_names,
        .normal.output_names            = outputs_names,
        .normal.input_num               = 1,              // len(input_names)
        .normal.output_num              = 1,              // len(output_names)
        .normal.mean                    = {127, 127, 127},
        .normal.norm                    = {0.0078125, 0.0078125, 0.0078125},
        .normal.scale                   = {10.872787},    //Only R329 has this option (r0p0 SDK)
    };

    // malloc buffer 
    float* output_buffer = (float*)malloc(out_fmap.w * out_fmap.h * out_fmap.c * sizeof(float));
    if(!output_buffer)
    {
        printf("no memory!!!\n");
        goto end;
    }

    //allocate quantized data buffer
    uint8_t* quantize_buffer = (uint8_t*)malloc(input.w * input.h * input.c);
    if(!quantize_buffer)
    {
        printf("no memory!!!\n");
        goto end;
    }
    out_fmap.data = output_buffer;
    input.buff_quantization = quantize_buffer;
#if TEST_IMAGE
    ret = loadFromBin("/root/test_input/input_data.bin", input.w * input.h * input.c, img->data);
    if(ret != 0)
    {
        printf("read file fail!\n");
        goto end;
    }
    img->mode = LIBMAIX_IMAGE_MODE_RGB888;`
#endif
    // nn model init
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
    // decoder init
    printf("-- yolo2 decoder create\n");
    yolo2_decoder = libmaix_nn_decoder_yolo2_create();
    if(!yolo2_decoder)
    {
        printf("no mem\n");
        goto end;
    }
    printf("-- yolo2 decoder init\n");
    err = yolo2_decoder->init(yolo2_decoder, (void*)&yolo2_config);
    if(err != LIBMAIX_ERR_NONE)
    {
        printf("decoder init error:%d\n", err);
        goto end;
    }
    libmaix_nn_decoder_yolo2_result_t yolo2_result;

    printf("-- start loop\n");
    while(1)
    {
#if !TEST_IMAGE
        // printf("--cam capture\n");
        CALC_TIME_START();
        err = cam->capture_image(cam,&img);
       
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

        input.data = (uint8_t *)img->data;
        err = nn->forward(nn, &input, &out_fmap);
        if(err != LIBMAIX_ERR_NONE)
        {
            printf("libmaix_nn forward fail: %s\n", libmaix_get_err_msg(err));
            break;
        }
     
        err = yolo2_decoder->decode(yolo2_decoder, &out_fmap, (void*)&yolo2_result);
        if(err != LIBMAIX_ERR_NONE)
        {
            printf("yolo2 decode fail: %s\n", libmaix_get_err_msg(err));
            goto end;
        }
      



        callback_arg.disp = disp;
        callback_arg.img = img;


        if(yolo2_result.boxes_num > 0)
        {
            printf("yolo2_result_boxes_num is %d \n",yolo2_result.boxes_num);
            
            libmaix_nn_decoder_yolo2_draw_result(yolo2_decoder, &yolo2_result, count++, labels, on_draw_box, (void*)&callback_arg);
        }
        // disp->draw(disp, img->data);
        err = libmaix_cv_image_resize(img, disp->width, disp->height, &show);  // resize the img to show on edge device 
        disp->draw_image(disp,show);
        // disp->flush(disp); // disp->draw last arg=1, means will call flush in draw functioin
        
        // printf("--convert test end\n");
        CALC_TIME_END("one image");


#if TEST_IMAGE
        break;
#endif
    }
end:
    if(yolo2_decoder)
    {
        yolo2_decoder->deinit(yolo2_decoder);
        libmaix_nn_decoder_yolo2_destroy(&yolo2_decoder);
    }
    if(output_buffer)
    {
        free(output_buffer);
    }
    if(nn)
    {
        libmaix_nn_destroy(&nn);
    }
    if(cam)
    {
        printf("--cam destory\n");
        libmaix_cam_destroy(&cam);
    }
    if(show)
    {
        printf("--image destory\n");
        libmaix_image_destroy(&show);
    }
    if(img)
    {
        printf("--image destory\n");
        libmaix_image_destroy(&img);
    }
    printf("--image module deinit\n");
    libmaix_nn_module_deinit();
    libmaix_image_module_deinit();
    libmaix_camera_module_deinit();
}

int main(int argc, char* argv[])
{
    struct libmaix_disp* disp = libmaix_disp_create(0);
    if(disp == NULL) {
        printf("creat disp object fail\n");
        return -1;
    }

    

    printf("draw test\n");
    nn_test(disp);
    printf("display end\n");

    libmaix_disp_destroy(&disp);
    return 0;
}

