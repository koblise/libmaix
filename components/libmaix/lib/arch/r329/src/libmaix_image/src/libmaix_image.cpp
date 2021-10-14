
#include "libmaix_image.h"
#include "libmaix_debug.h"

// inline static unsigned short make16color(unsigned char r, unsigned char g, unsigned char b)
// {
// 	return (
// 	(((r >> 3) & 31) << 11) |
// 	(((g >> 2) & 63) << 5)  |
// 	 ((b >> 3) & 31)		);
// }

libmaix_err_t libmaix_image_soft_convert(struct libmaix_image *obj, libmaix_image_mode_t mode, struct libmaix_image **new_img)
{
  switch (mode)
  {
      case LIBMAIX_IMAGE_MODE_RGB565:
        if (obj->mode == LIBMAIX_IMAGE_MODE_RGB888) {
          if (obj == *new_img || obj->width != (*new_img)->width || obj->height != (*new_img)->height) return LIBMAIX_ERR_PARAM;
          uint8_t *rgb888 = (uint8_t *)obj->data;
          uint16_t *rgb565 = (uint16_t *)(*new_img)->data;
          for (uint16_t *end = rgb565 + obj->width * obj->height; rgb565 < end; rgb565 += 1, rgb888 += 3) {
            // *rgb565 = make16color(rgb888[0], rgb888[1], rgb888[2]);
            *rgb565 = ((((rgb888[0] >> 3) & 31) << 11) | (((rgb888[1] >> 2) & 63) << 5) | ((rgb888[2] >> 3) & 31));
          }
          return LIBMAIX_ERR_NONE;
        } else {
          ;
        }
        break;
      
      default:
        break;
  }

  return LIBMAIX_ERR_NOT_IMPLEMENT;
}

libmaix_err_t libmaix_image_soft_resize(struct libmaix_image *obj, int w, int h, struct libmaix_image **new_img)
{
  LIBMAIX_IMAGE_ERROR(LIBMAIX_ERR_NOT_IMPLEMENT);
  return LIBMAIX_ERR_NOT_IMPLEMENT;
}

libmaix_err_t libmaix_image_soft_crop(struct libmaix_image *obj, int x, int y, int w, int h, struct libmaix_image **new_img)
{
  LIBMAIX_IMAGE_ERROR(LIBMAIX_ERR_NOT_IMPLEMENT);
  return LIBMAIX_ERR_NOT_IMPLEMENT;
}

#ifdef __cplusplus
extern "C"
{
#endif

#include "stdlib.h"

  libmaix_err_t libmaix_image_module_init()
  {
    // return libmaix_image_hal_module_init();
  }

  libmaix_err_t libmaix_image_module_deinit()
  {
    // return libmaix_image_hal_module_deinit();
  }

  libmaix_image_t *libmaix_image_create(int w, int h, libmaix_image_mode_t mode, libmaix_image_layout_t layout, void *data, bool is_data_alloc)
  {
    uint64_t img_size = 0;

    if (!(mode == LIBMAIX_IMAGE_MODE_RGB888 ||
          mode == LIBMAIX_IMAGE_MODE_YUV420SP_NV21 ||
          mode == LIBMAIX_IMAGE_MODE_RGB565 ||
          mode == LIBMAIX_IMAGE_MODE_RGBA8888))
    {
      LIBMAIX_IMAGE_ERROR(LIBMAIX_ERR_PARAM);
      return NULL;
    }
    if (!(w == 0 || h == 0 || mode == LIBMAIX_IMAGE_MODE_INVALID))
    {
      if (!data)
      {
        switch (mode)
        {
        case LIBMAIX_IMAGE_MODE_RGB565:
          img_size = w * h * 2;
          break;
        case LIBMAIX_IMAGE_MODE_RGB888:
          img_size = w * h * 3;
          break;
        case LIBMAIX_IMAGE_MODE_RGBA8888:
          img_size = w * h * 3;
          break;
        case LIBMAIX_IMAGE_MODE_YUV420SP_NV21:
          img_size = w * h * 3 / 2;
          break;
        default:
          LIBMAIX_IMAGE_ERROR(LIBMAIX_ERR_PARAM);
          return NULL;
        }
        data = malloc(img_size);
        if (!data)
        {
          LIBMAIX_IMAGE_ERROR(LIBMAIX_ERR_NO_MEM);
          return NULL;
        }
        is_data_alloc = true;
      }
    }
    libmaix_image_t *img = (libmaix_image_t *)malloc(sizeof(libmaix_image_t));
    if (!img)
    {
      LIBMAIX_IMAGE_ERROR(LIBMAIX_ERR_NO_MEM);
      return NULL;
    }
    img->width = w;
    img->height = h;
    img->mode = mode;
    img->layout = layout;
    img->data = data;
    img->is_data_alloc = is_data_alloc;

    img->convert = libmaix_image_soft_convert;
    // img->draw_rectangle = libmaix_image_soft_draw_rectangle;
    // img->draw_string    = libmaix_image_soft_draw_string;
    img->resize = libmaix_image_soft_resize;
    img->crop = libmaix_image_soft_crop;
    return img;
  }

  void libmaix_image_destroy(libmaix_image_t **obj)
  {
    if (*obj)
    {
      if ((*obj)->is_data_alloc)
      {
        free((*obj)->data);
        (*obj)->data = NULL;
      }
      free(*obj);
      *obj = NULL;
    }
  }

#ifdef __cplusplus
}
#endif
