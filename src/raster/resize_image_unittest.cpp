// Aseprite Gfx Library
// Copyright (C) 2001-2013 David Capello
//
// This source file is distributed under MIT license,
// please read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <gtest/gtest.h>

#include "raster/color.h"
#include "raster/image.h"
#include "raster/algorithm/resize_image.h"

using namespace std;
using namespace raster;

/***************************
 * Test dat
 */

// Base image
color_t test_image_base_3x3[9] =
{
  0xff000000, 0xffffffff, 0xff000000,
  0xffffffff, 0xffffffff, 0xffffffff,
  0xff000000, 0xffffffff, 0xff000000
};

// Base image scaled to 9x9 with nearest neighbor interpolation
color_t test_image_scaled_9x9_nearest[81] =
{
  0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xff000000,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xff000000
};

color_t test_image_scaled_9x9_bilinear[81] =
{
  0xff000000, 0xff000000, 0xff555555, 0xffaaaaaa, 0xffffffff, 0xffa9a9a9, 0xff555555, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff555555, 0xffaaaaaa, 0xffffffff, 0xffa9a9a9, 0xff555555, 0xff000000, 0xff000000,
  0xff555555, 0xff555555, 0xff8d8d8d, 0xffc6c6c6, 0xffffffff, 0xffc5c5c5, 0xff8d8d8d, 0xff555555, 0xff555555,
  0xffaaaaaa, 0xffaaaaaa, 0xffc6c6c6, 0xffe2e2e2, 0xffffffff, 0xffe2e2e2, 0xffc6c6c6, 0xffaaaaaa, 0xffaaaaaa,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffa9a9a9, 0xffa9a9a9, 0xffc6c6c6, 0xffe2e2e2, 0xffffffff, 0xffe2e2e2, 0xffc6c6c6, 0xffa9a9a9, 0xffa9a9a9,
  0xff555555, 0xff555555, 0xff8d8d8d, 0xffc6c6c6, 0xffffffff, 0xffc5c5c5, 0xff8d8d8d, 0xff555555, 0xff555555,
  0xff000000, 0xff000000, 0xff555555, 0xffaaaaaa, 0xffffffff, 0xffa9a9a9, 0xff555555, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff555555, 0xffaaaaaa, 0xffffffff, 0xffa9a9a9, 0xff555555, 0xff000000, 0xff000000
};

Image* create_image_from_data(PixelFormat format, color_t* data, int width, int height) 
{
  Image* new_image = Image::create(format, width, height);
  for (int i = 0; i < width * height; i++) {
    new_image->putPixel(i % width, i / width, data[i]);
  }

  return new_image;
}

// Simple pixel to pixel image comparison
bool compare_images(Image* a, Image* b)
{
  for (int y = 0; y < a->getHeight(); y++) {
    for (int x = 0; x < a->getWidth(); x++) {
      printf("(%d,%d) [%08x]==[%08x]\n", x, y, a->getPixel(x, y), b->getPixel(x, y));
      if (a->getPixel(x, y) != b->getPixel(x, y))
        return false;
    }
  }
  return true;
}

TEST(ResizeImage, NearestNeighborInterp)
{
  Image* src = create_image_from_data(IMAGE_RGB, test_image_base_3x3, 3, 3);
  Image* dst = Image::create(IMAGE_RGB, 9, 9);

  // Pre-rendered test image for comparison
  Image* test_dst = create_image_from_data(IMAGE_RGB, test_image_scaled_9x9_nearest, 9, 9);

  algorithm::resize_image(src, dst, algorithm::ResizeMethod::RESIZE_METHOD_NEAREST_NEIGHBOR, NULL, NULL);

  ASSERT_TRUE(compare_images(dst, test_dst)) << "resize_image() result does not match test image!";
}

TEST(ResizeImage, BilinearInterpRGBType)
{
  Image* src = create_image_from_data(IMAGE_RGB, test_image_base_3x3, 3, 3);
  Image* dst = Image::create(IMAGE_RGB, 9, 9);

  // Pre-rendered test image for comparison
  Image* test_dst = create_image_from_data(IMAGE_RGB, test_image_scaled_9x9_bilinear, 9, 9);

  algorithm::resize_image(src, dst, algorithm::ResizeMethod::RESIZE_METHOD_BILINEAR, NULL, NULL);

  ASSERT_TRUE(compare_images(dst, test_dst)) << "resize_image() result does not match test image!";
}
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
