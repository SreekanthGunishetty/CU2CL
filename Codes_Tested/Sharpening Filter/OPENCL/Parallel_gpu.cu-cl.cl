

#include "device_launch_parameters.h-cl.cl"


#include "opencv2/imgproc/imgproc.hpp-cl.cl"






//
#define BLOCK_SIZE      16
#define FILTER_WIDTH    3       
#define FILTER_HEIGHT   3       

using namespace std;
using namespace cv;








// Run Sharpening Filter on GPU
__kernel void sharpeningFilter(__global unsigned char* srcImage, __global unsigned char* dstImage, unsigned int width, unsigned int height, int channel)
{
    int x = get_group_id(0) * get_local_size(0) + get_local_id(0);
    int y = get_group_id(1) * get_local_size(1) + get_local_id(1);

    float kernel[FILTER_WIDTH][FILTER_HEIGHT] = { -1, -1, -1, -1, 9, -1, -1, -1, -1 };
    // only threads inside image will write results
    if ((x >= FILTER_WIDTH / 2) && (x < (width - FILTER_WIDTH / 2)) && (y >= FILTER_HEIGHT / 2) && (y < (height - FILTER_HEIGHT / 2)))
    {
        for (int c = 0; c < channel; c++)
        {
            // Sum of pixel values 
            float sum = 0;
            // Loop inside the filter to average pixel values
            for (int ky = -FILTER_HEIGHT / 2; ky <= FILTER_HEIGHT / 2; ky++) {
                for (int kx = -FILTER_WIDTH / 2; kx <= FILTER_WIDTH / 2; kx++) {
                    float fl = srcImage[((y + ky) * width + (x + kx)) * channel + c];
                    sum += fl * kernel[ky + FILTER_HEIGHT / 2][kx + FILTER_WIDTH / 2];
                }
            }
            dstImage[(y * width + x) * channel + c] = sum;
        }
    }
}


// The wrapper is used to call sharpening filter 

