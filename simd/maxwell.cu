%%writefile Sources/maxwell.cu
#include "dli.h"
#include <thrust/device_vector.h>
#include <thrust/transform.h>

void update_hx(int n, float dx, float dy, float dt, thrust::device_vector<float> &hx,
               thrust::device_vector<float> &ez) {
  // Create zip iterator for (ez[i+n], ez[i])
  auto zip_begin = thrust::make_zip_iterator(
      thrust::make_tuple(ez.begin() + n, ez.begin()));

  // Transform iterator computes ez[i+n] - ez[i]
  auto diff_begin = thrust::make_transform_iterator(
      zip_begin,
      [] __device__ (thrust::tuple<float, float> t) -> float {
        return thrust::get<0>(t) - thrust::get<1>(t);
      });

  // Update hx using computed difference
  thrust::transform(
      thrust::device,
      hx.begin(),
      hx.end() - n,
      diff_begin,
      hx.begin(),
      [dt, dx, dy] __device__ (float h, float cex) -> float {
        return h - dli::C0 * dt / 1.3f * cex / dy;
      });
}

void update_hy(int n, float dx, float dy, float dt, thrust::device_vector<float> &hy,
               thrust::device_vector<float> &ez) {
  // Create zip iterator for (ez[i], ez[i+1])
  auto zip_begin = thrust::make_zip_iterator(
      thrust::make_tuple(ez.begin(), ez.begin() + 1));

  // Transform iterator computes ez[i+1] - ez[i]
  auto diff_begin = thrust::make_transform_iterator(
      zip_begin,
      [] __device__ (thrust::tuple<float, float> t) -> float {
        return thrust::get<0>(t) - thrust::get<1>(t);
      });

  // Update hy using computed difference
  thrust::transform(
      thrust::device,
      hy.begin(),
      hy.end() - 1,
      diff_begin,
      hy.begin(),
      [dt, dx, dy] __device__ (float h, float cey) -> float {
          return h - dli::C0 * dt / 1.3f * cey / dx;
      });
}

void update_dz(int n, float dx, float dy, float dt, thrust::device_vector<float> &hx_vec,
               thrust::device_vector<float> &hy_vec, thrust::device_vector<float> &dz_vec) {

  float* hx_ptr = thrust::raw_pointer_cast(hx_vec.data());
  float* hy_ptr = thrust::raw_pointer_cast(hy_vec.data());
  float* dz_ptr = thrust::raw_pointer_cast(dz_vec.data());

  auto hx = hx_vec.begin();
  auto hy = hy_vec.begin();
  auto dz = dz_vec.begin();
  thrust::for_each(thrust::device,
                  thrust::make_counting_iterator<int>(0),
                  thrust::make_counting_iterator<int>(n*n),
                  [=] __device__ (int cell_id) {
                  if (cell_id > n) {
                    float hx_diff = hx_ptr[cell_id - n] - hx_ptr[cell_id];
                    float hy_diff = hy_ptr[cell_id] - hy_ptr[cell_id - 1];
                    dz[cell_id] += dli::C0 * dt * (hx_diff / dx + hy_diff / dy);
                  }
                });
}

void update_ez(thrust::device_vector<float> &ez, thrust::device_vector<float> &dz) {
  thrust::transform(thrust::device, dz.begin(), dz.end(), ez.begin(),
                 [] __device__(float d) -> float {return d / 1.3f; });
}

void simulate(int cells_along_dimension, float dx, float dy, float dt,
              thrust::device_vector<float> &d_hx,
              thrust::device_vector<float> &d_hy,
              thrust::device_vector<float> &d_dz,
              thrust::device_vector<float> &d_ez) {
  for (int step = 0; step < dli::steps; step++) {
    update_hx(cells_along_dimension, dx, dy, dt, d_hx, d_ez);
    update_hy(cells_along_dimension, dx, dy, dt, d_hy, d_ez);
    update_dz(cells_along_dimension, dx, dy, dt, d_hx, d_hy, d_dz);
    update_ez(d_ez, d_dz);
  }
}
