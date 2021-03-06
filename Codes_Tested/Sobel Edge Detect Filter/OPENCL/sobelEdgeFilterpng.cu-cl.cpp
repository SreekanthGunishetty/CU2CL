#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "cu2cl_util.h"




cl_kernel __cu2cl_Kernel_sobelGpu;
cl_program __cu2cl_Program_sobelEdgeFilterpng_cu;
extern const char *progSrc;
extern size_t progLen;

extern cl_kernel __cu2cl_Kernel___cu2cl_Memset;
extern cl_program __cu2cl_Util_Program;
extern cl_platform_id __cu2cl_Platform;
extern cl_device_id __cu2cl_Device;
extern cl_context __cu2cl_Context;
extern cl_command_queue __cu2cl_CommandQueue;

extern size_t globalWorkSize[3];
extern size_t localWorkSize[3];
void __cu2cl_Cleanup_sobelEdgeFilterpng_cu() {
    clReleaseKernel(__cu2cl_Kernel_sobelGpu);
    clReleaseProgram(__cu2cl_Program_sobelEdgeFilterpng_cu);
}
void __cu2cl_Init_sobelEdgeFilterpng_cu() {
    #ifdef WITH_ALTERA
    progLen = __cu2cl_LoadProgramSource("sobelEdgeFilterpng_cu_cl.aocx", &progSrc);
    __cu2cl_Program_sobelEdgeFilterpng_cu = clCreateProgramWithBinary(__cu2cl_Context, 1, &__cu2cl_Device, &progLen, (const unsigned char **)&progSrc, NULL, NULL);
    #else
    progLen = __cu2cl_LoadProgramSource("sobelEdgeFilterpng.cu-cl.cl", &progSrc);
    __cu2cl_Program_sobelEdgeFilterpng_cu = clCreateProgramWithSource(__cu2cl_Context, 1, &progSrc, &progLen, NULL);
    #endif
    free((void *) progSrc);
    clBuildProgram(__cu2cl_Program_sobelEdgeFilterpng_cu, 1, &__cu2cl_Device, "-I . ", NULL, NULL);
    __cu2cl_Kernel_sobelGpu = clCreateKernel(__cu2cl_Program_sobelEdgeFilterpng_cu, "sobelGpu", NULL);
}

#include<iostream>
#include<stdio.h>
#include "math.h-cl.h"
#include "lodepng.h-cl.h"
#include <string>
#include <sys/time.h>
#define N 20.0

// nvcc sobelEdgeFilterpng.cu lodepng.cpp -arch sm_20
// Time taken by CPU :  0.95400 ms
// Time taken by GPU :  0.09100 ms


typedef unsigned char byte;

// making a structure for the image with pixels, width and height
struct imgData {
  imgData(byte* pix = NULL, unsigned int w = 0, unsigned int h = 0) : pixels(pix), width(w), height(h) {};
  byte* pixels;
  unsigned int width;
  unsigned int height;
};

imgData loadImage(char* filename) {
  unsigned int width, height;
  byte* rgb;
  unsigned error = lodepng_decode_file(&rgb, &width, &height, filename, LCT_RGBA, 8);
  if (error) {
    printf("LodePNG had an error during file processing. Exiting program.\n");
    // printf("Error code: %u: %s\n", error, lodepng_error_text(error));
    exit(2);
  }
  // we convert to grayscale here
  byte* grayscale = new byte[width * height];
  // pixels stored as rgba
  byte* img = rgb;
  for (int i = 0; i < width * height; ++i) {
    int r = *img++;
    int g = *img++;
    int b = *img++;
    int a = *img++;
    grayscale[i] = 0.3 * r + 0.6 * g + 0.1 * b + 0.5;
  }
  free(rgb);
  return imgData(grayscale, width, height);
}


void writeImage(char* filename, imgData img) {
  std::string newName = filename;
  // checking image coordinates
  // printf("starting to write %d is width and %d is height\n", img.width, img.height);
  unsigned error = lodepng_encode_file(newName.c_str(), img.pixels, img.width, img.height, LCT_GREY, 8);
  if (error) {
    printf("LodePNG had an error during file writing. Exiting program.\n");
    exit(3);
  }
  else
  {
    printf("No error while writing?\n");
  }
  delete [] img.pixels;
}



void sobelCpu(byte* orig, byte* op, const unsigned int width, const unsigned int height) {
  for (int y = 1; y < height - 1; y++) {
    for (int x = 1; x < width - 1; x++) {
      // we find dx and dy
      int dx = (-1 * orig[(y - 1) * width + (x - 1)]) + (-2 * orig[y * width + (x - 1)]) + (-1 * orig[(y + 1) * width + (x - 1)]) +
      (orig[(y - 1) * width + (x + 1)]) + (2 * orig[y * width + (x + 1)]) + (orig[(y + 1) * width + (x + 1)]);
      int dy = (orig[(y - 1) * width + (x - 1)]) + (2 * orig[(y - 1) * width + x]) + (orig[(y - 1) * width + (x + 1)]) +
      (-1 * orig[(y + 1) * width + (x - 1)]) + (-2 * orig[(y + 1) * width + x]) + (-1 * orig[(y + 1) * width + (x + 1)]);
      // sqrt(dx^2 + dy^2) gives the new pixel value
      op[y * width + x] = sqrt((float)((dx * dx) + (dy * dy)));
    }
  }
}





int main()
{
__cu2cl_Init();

	cl_mem d_in;
cl_mem d_out;

  imgData in = loadImage("person.png");
  
  int w = in.width;int h=in.height;
  
  // checking dimensions
  // printf("width is %d, height is %d\n",w,h);
  
  imgData op(new byte[w*h],w,h);
  imgData op2(new byte[w*h],w,h);

  // debuggin to check about the 
  // writeImage("personcameas.png", in);

  *(void **)&d_in = clCreateBuffer(__cu2cl_Context, CL_MEM_READ_WRITE, (w*h), NULL, NULL);
  *(void **)&d_out = clCreateBuffer(__cu2cl_Context, CL_MEM_READ_WRITE, (w*h), NULL, NULL);

  clEnqueueWriteBuffer(__cu2cl_CommandQueue, d_in, CL_TRUE, 0, w*h, in.pixels, 0, NULL, NULL);
  __cu2cl_Memset(d_out, 0, w*h);

  size_t threadsPerBlock[3] = {N, N, 1};
  size_t numBlocks[3] = {ceil(w/N), ceil(h/N), 1};


  struct timeval t1, t2;
  gettimeofday(&t1, 0);
  sobelCpu(in.pixels,op.pixels,w,h);
  gettimeofday(&t2, 0);

  double time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;

  printf("Time taken by CPU :  %3.5f ms \n", time);
  
  // printf("before writing to cpuimg\n");
  writeImage("fromcpu.png", op);

  // printf("after writing to cpuimg\n");
  // writeImage("aftercpu.png", in);
  
  gettimeofday(&t1, 0);
  clSetKernelArg(__cu2cl_Kernel_sobelGpu, 0, sizeof(cl_mem), &d_in);
clSetKernelArg(__cu2cl_Kernel_sobelGpu, 1, sizeof(cl_mem), &d_out);
clSetKernelArg(__cu2cl_Kernel_sobelGpu, 2, sizeof(unsigned int), &w);
clSetKernelArg(__cu2cl_Kernel_sobelGpu, 3, sizeof(unsigned int), &h);
localWorkSize[0] = threadsPerBlock[0];
localWorkSize[1] = threadsPerBlock[1];
localWorkSize[2] = threadsPerBlock[2];
globalWorkSize[0] = numBlocks[0]*localWorkSize[0];
globalWorkSize[1] = numBlocks[1]*localWorkSize[1];
globalWorkSize[2] = numBlocks[2]*localWorkSize[2];
clEnqueueNDRangeKernel(__cu2cl_CommandQueue, __cu2cl_Kernel_sobelGpu, 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
  
  // checking if any errors occured
/*CU2CL Unsupported -- Unsupported CUDA call: cudaDeviceSynchronize*/
  cudaError_t cudaerror = cudaDeviceSynchronize(); // waits for completion, returns error code
  if ( cudaerror != cudaSuccess ) fprintf( stderr, "Cuda failed to synchronize:\n"); // if error, output error
  gettimeofday(&t2, 0);
  
  // printf("survived kernel\n");
  time = (1000000.0*(t2.tv_sec-t1.tv_sec) + t2.tv_usec-t1.tv_usec)/1000.0;
  printf("Time taken by GPU :  %3.5f ms \n", time);

  // writeImage("aftergpucpupersoncameas.png", in);
  clEnqueueReadBuffer(__cu2cl_CommandQueue, d_out, CL_TRUE, 0, (w*h), op2.pixels, 0, NULL, NULL);
  op2.width=w;
  op2.height=h;
  
  writeImage("fromgpu.png", op2);
  
__cu2cl_Cleanup();
}

