%%writefile Sources/coarse.cu
#include "dli.h"

__global__ void kernel(dli::temperature_grid_f fine,
                       dli::temperature_grid_f coarse) {
  int coarse_row = blockIdx.x / coarse.extent(1);
  int coarse_col = blockIdx.x % coarse.extent(1);
  int row = threadIdx.x / dli::tile_size;
  int col = threadIdx.x % dli::tile_size;
  int fine_row = coarse_row * dli::tile_size + row;
  int fine_col = coarse_col * dli::tile_size + col;

  float thread_value = fine(fine_row, fine_col);

  using BlockReduce = cub::BlockReduce<float, dli::block_threads>;
  __shared__ typename BlockReduce::TempStorage temp_storage;
  // Compute the block-wide sum for thread0
  float block_sum = BlockReduce(temp_storage).Sum(thread_value);

  if(threadIdx.x == 0){
    float block_average = block_sum / (dli::tile_size * dli::tile_size);
    coarse(coarse_row, coarse_col) = block_average;
  }
}

void coarse(dli::temperature_grid_f fine, dli::temperature_grid_f coarse) {
  kernel<<<coarse.size(), dli::block_threads>>>(fine, coarse);
}
