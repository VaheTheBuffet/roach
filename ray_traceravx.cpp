#include "material.h"
#include <cassert>
#include <xmmintrin.h>
#define NDEBUG
#define DEBUG_LOG

#include <iostream>
#include <immintrin.h>

#include "ray.h"
#include "vec.h"
#include "sphere.h"
#include "ctx.h"
#include "video.h"
#include "util.h"

#include "ray_tracer.h"

using path_meta_t = std::uint32_t;
using path_meta_t_v = __m256i;

using u32 = std::uint32_t;
using u8 = std::uint8_t;
using i32 = int;
using f32 = float;

#define STACK_SIZE 64
#define MAX_DEPTH 4
#define N_LANES 8

#define LANE_WIDTH (sizeof(f32))

#define ZERO_PS (_mm256_setzero_ps())
#define NEG_ZERO_PS (_mm256_set1_ps(-0.f))
#define ONE_PS (_mm256_set1_ps(1.f))
#define NEG_ONE_PS (_mm256_set1_ps(-1.f))
#define TWO_PS (_mm256_set1_ps(2.f))
#define EPS_PS (_mm256_set1_ps(1e-2f))
// generates an arithemtic progression with step size 1 starting at f
#define PROG_PS(f) (_mm256_setr_ps(f, f+1.f, f+2.f, f+3.f, f+4.f, f+5.f, f+6.f, f+7))

#define ONE_EPI32 (_mm256_set1_epi32(1))
#define NEG_ONE_EPI32 (_mm256_set1_epi32(-1))
#define ZERO_EPI32 (_mm256_set1_epi32(0))
#define PROG_EPI32(n) (_mm256_setr_epi32(n, n+1, n+2, n+3, n+4, n+5, n+6, n+7))

#define assert_all(__M) (                        \
    assert(_mm256_extract_epi32(__M, 0) == -1 && \
           _mm256_extract_epi32(__M, 1) == -1 && \
           _mm256_extract_epi32(__M, 2) == -1 && \
           _mm256_extract_epi32(__M, 3) == -1 && \
           _mm256_extract_epi32(__M, 4) == -1 && \
           _mm256_extract_epi32(__M, 5) == -1 && \
           _mm256_extract_epi32(__M, 6) == -1 && \
           _mm256_extract_epi32(__M, 7) == -1))

#define MAT_LEN (static_cast<i32>(material_type::LENGTH))

#define rand_f (static_cast<f32>(rand()) / static_cast<f32>(RAND_MAX))

#define SIZE (1920 * 1080)
struct path_data_t {
    alignas(32) f32 ray_orig_x[SIZE];
    alignas(32) f32 ray_orig_y[SIZE];
    alignas(32) f32 ray_orig_z[SIZE];

    alignas(32) f32 ray_dir_x[SIZE];
    alignas(32) f32 ray_dir_y[SIZE];
    alignas(32) f32 ray_dir_z[SIZE];

    alignas(32) f32 color_r[SIZE];
    alignas(32) f32 color_g[SIZE];
    alignas(32) f32 color_b[SIZE];

    alignas(32) path_meta_t meta[SIZE];
};

struct index_buffer_t {
    u32 idx[SIZE];

    u32 n;
};
#undef SIZE

struct path_batch_t {
    __m256 ray_orig_x;
    __m256 ray_orig_y;
    __m256 ray_orig_z;

    __m256 ray_dir_x;
    __m256 ray_dir_y;
    __m256 ray_dir_z;

    __m256 color_r;
    __m256 color_g;
    __m256 color_b;

    __m256i meta;

    u8 active;
};

struct qr_avx_t {
    __m256 r1;
    __m256 r2;
    __m256 nan;
};

//---------------------global----------------------//
static path_data_t s;
static path_batch_t pb;

static index_buffer_t ibo1[MAT_LEN];
static index_buffer_t ibo2[MAT_LEN];
static index_buffer_t *ibo_batch = ibo1;
static index_buffer_t *next_ibo_batch = ibo2;
static index_buffer_t *ibo = ibo1;

#define SPHERE_NULL 0
#define SPHERE_FACTOR_MAX 100000.f
#define MAX_SPHERES 64
alignas(32) static f32 sphere_center_x[MAX_SPHERES];
alignas(32) static f32 sphere_center_y[MAX_SPHERES];
alignas(32) static f32 sphere_center_z[MAX_SPHERES];
alignas(32) static f32 sphere_radius[MAX_SPHERES];
alignas(32) static f32 sphere_albedo_r[MAX_SPHERES];
alignas(32) static f32 sphere_albedo_g[MAX_SPHERES];
alignas(32) static f32 sphere_albedo_b[MAX_SPHERES];
alignas(32) static f32 sphere_ior[MAX_SPHERES];
alignas(32) static f32 sphere_reflectance[MAX_SPHERES];

#define MAX_LIGHTS 10
static vec3 lights[MAX_LIGHTS];
static i32 n_lights;

static qr_avx_t quadratic_res;
static sphere spheres[MAX_SPHERES];
static i32 sphere_top;

static Ctx *ctx;

static f32 off_x;
static f32 off_y;
static i32 n_samples = 1;
//---------------------global----------------------//

// this entire idea was honestly the stupidest shit ever
#define MASK0 1u
#define MASK0_EPI32 _mm256_set1_epi32(MASK0)
#define START1 1u
#define START1_EPI32 _mm256_set1_epi32(START1)
#define MASK1 3u
#define MASK1_EPI32 _mm256_set1_epi32(MASK1)
#define START2 3u
#define START2_EPI32 _mm256_set1_epi32(START2)
#define MASK2 0xFFFFFFF8u
#define MASK2_EPI32 _mm256_set1_epi32(MASK2)
// None of these truncate the input, it can overwrite past its boundary
static inline path_meta_t path_meta_face(path_meta_t p, path_meta_t f) {
    return (p & ~MASK0) | (f & MASK0);
}

static inline path_meta_t path_meta_face(path_meta_t p) {
    return p & MASK0;
}

static inline path_meta_t path_meta_depth(path_meta_t p, path_meta_t d) {
    return (p & ~MASK1) | (d << START1);
}

static inline path_meta_t path_meta_depth(path_meta_t p) {
    return (p & MASK1) >> START1;
}

static inline path_meta_t path_meta_sphere(path_meta_t p, path_meta_t s) {
    return (p & ~MASK2) | (s << START2);
}

static inline path_meta_t path_meta_sphere(path_meta_t p) {
    return (p & MASK2) >> START2;
}

static inline path_meta_t_v path_meta_face(path_meta_t_v p, path_meta_t_v f) {
    p = _mm256_andnot_si256(MASK0_EPI32, p);
    p = _mm256_or_si256(p, _mm256_and_si256(MASK0_EPI32, f));
    return p;
}

static inline path_meta_t_v path_meta_face(path_meta_t_v p) {
    return _mm256_and_si256(p, MASK0_EPI32);
}

static inline path_meta_t_v path_meta_depth(path_meta_t_v p, path_meta_t_v d) {
    p = _mm256_andnot_si256(MASK1_EPI32, p);
    p = _mm256_or_si256(p, _mm256_slli_epi32(d, START1));
    return p;
}

static inline path_meta_t_v path_meta_depth(path_meta_t_v p) {
    p = _mm256_and_si256(p, _mm256_set1_epi32(MASK1));
    p = _mm256_srli_epi32(p, START1);
    return p;
}

static inline path_meta_t_v path_meta_sphere(path_meta_t_v p, path_meta_t_v s) {
    p = _mm256_and_si256(p, _mm256_set1_epi32(~MASK2));
    p = _mm256_or_si256(p, _mm256_slli_epi32(s, START2));
    return p;
}

static inline path_meta_t_v path_meta_sphere(path_meta_t_v p) {
    p = _mm256_and_si256(p, _mm256_set1_epi32(MASK2));
    p = _mm256_srai_epi32(p, START2);
    return p;
}
#undef MASK0
#undef START1
#undef MASK1
#undef START2
#undef MASK2

static inline f32 _mm256_extract_ps(__m256 m, i32 i) {
    __m256 v = _mm256_permutevar8x32_ps(m, _mm256_set1_epi32(i));
    return _mm256_cvtss_f32(v);
}

static inline i32 _mm256_extractvar_epi32(__m256i m, i32 i) {
    __m256i v = _mm256_permutevar8x32_epi32(m, _mm256_set1_epi32(i));
    return _mm256_cvtsi256_si32(v);
}

static inline void sync_sphere_tables() 
{
    for (i32 s = 0; s < sphere_top; ++s) {
        sphere_center_x[s] = spheres[s].center.x();
        sphere_center_y[s] = spheres[s].center.y();
        sphere_center_z[s] = spheres[s].center.z();
        sphere_radius[s] = spheres[s].r;
        sphere_albedo_r[s] = spheres[s].mat.albedo.x();
        sphere_albedo_g[s] = spheres[s].mat.albedo.y();
        sphere_albedo_b[s] = spheres[s].mat.albedo.z();
        sphere_ior[s] = spheres[s].mat.refractive_index;
        sphere_reflectance[s] = spheres[s].mat.reflectance;
    }
}

#define V_RAND_MAX (_mm256_cvtepi32_ps(_mm256_set1_epi32(RAND_MAX)))
static inline __m256 _mm256_rand_ps() {
    static __m256i seed = PROG_EPI32(1);

    seed = _mm256_xor_si256(seed, _mm256_slli_epi32(seed, 13));
    seed = _mm256_xor_si256(seed, _mm256_srli_epi32(seed, 17));
    seed = _mm256_xor_si256(seed, _mm256_slli_epi32(seed, 5));

    return _mm256_div_ps(_mm256_cvtepi32_ps(seed), V_RAND_MAX);
}
#undef V_RAND_MAX

// stop execution if ray is at target depth
#if 0
static bool debug_catch_ray_depth(state_t &s, i32 d) 
{
    if (s.depths[s.path] == d) {
        --s.stack_top;
        s.coord_color += s.colors[s.path];
        return true;
    } else if (s.hit_sphere == -1) {
        --s.stack_top;
        return true;
    } else if (s.depths[s.path] == 4) {
        --s.stack_top;
        return true;
    }

    return false;
}
#endif

/*
 * find intersections between batch of rays and sphere
 *
 * @in param sphere
 * @in state pb.ray
 * @out state quadratic_res
 */
static void sphere_intersection(const sphere &sphere) 
{
    __m256 relative_center_x = _mm256_sub_ps(_mm256_set1_ps(sphere.center.x()), pb.ray_orig_x);
    __m256 relative_center_y = _mm256_sub_ps(_mm256_set1_ps(sphere.center.y()), pb.ray_orig_y);
    __m256 relative_center_z = _mm256_sub_ps(_mm256_set1_ps(sphere.center.z()), pb.ray_orig_z);

    __m256 a = _mm256_add_ps(_mm256_mul_ps(pb.ray_dir_x, pb.ray_dir_x),
        _mm256_add_ps(_mm256_mul_ps(pb.ray_dir_y, pb.ray_dir_y),
            _mm256_mul_ps(pb.ray_dir_z, pb.ray_dir_z)));

    __m256 h = _mm256_add_ps(_mm256_mul_ps(pb.ray_dir_x, relative_center_x),
        _mm256_add_ps(_mm256_mul_ps(pb.ray_dir_y, relative_center_y),
            _mm256_mul_ps(pb.ray_dir_z, relative_center_z)));

    __m256 c = _mm256_sub_ps(
        _mm256_add_ps(_mm256_mul_ps(relative_center_x, relative_center_x),
            _mm256_add_ps(_mm256_mul_ps(relative_center_y, relative_center_y),
                _mm256_mul_ps(relative_center_z, relative_center_z))),
        _mm256_set1_ps(sphere.r * sphere.r));

    __m256 disc_2 = _mm256_sub_ps(_mm256_mul_ps(h, h), _mm256_mul_ps(a, c));
    __m256 invalid = _mm256_cmp_ps(disc_2, ZERO_PS, _CMP_LT_OQ);
    __m256 safe_disc_2 = _mm256_max_ps(disc_2, ZERO_PS);
    __m256 disc = _mm256_sqrt_ps(safe_disc_2);

    quadratic_res.r1 = _mm256_div_ps(_mm256_sub_ps(h, disc), a);
    quadratic_res.r2 = _mm256_div_ps(_mm256_add_ps(h, disc), a);
    quadratic_res.nan = invalid;
}

/*
 * intersect batch of rays with closest spheres
 *
 * @in state quadratic_res
 * @out state min_factor
 * @out state hit_sphere
 * @out state forward
 */
static void intersect_batch() 
{
    __m256i hit_sphere = _mm256_set1_epi32(SPHERE_NULL);
    __m256 min_factor = _mm256_set1_ps(SPHERE_FACTOR_MAX);
    __m256 forward = ZERO_PS;

    for (auto s = 0; s < sphere_top; ++s) {
        if (s == SPHERE_NULL)
            continue;

        __m256i i_lane = _mm256_set1_epi32(s);
        __m256 valid;
        __m256 positive;
        __m256 new_min;

        sphere_intersection(spheres[s]);

        //check low root
        valid = _mm256_cmp_ps(quadratic_res.nan, ZERO_PS, _CMP_EQ_OQ);
        positive = _mm256_cmp_ps(quadratic_res.r1, ZERO_PS, _CMP_GT_OQ);
        new_min = _mm256_and_ps(valid, positive);
        new_min = _mm256_and_ps(new_min, _mm256_cmp_ps(quadratic_res.r1, min_factor, _CMP_LT_OQ));

        min_factor = _mm256_blendv_ps(min_factor, quadratic_res.r1, new_min);
        hit_sphere = _mm256_blendv_epi8(hit_sphere, i_lane, _mm256_castps_si256(new_min));
        forward = _mm256_or_ps(forward, new_min);

        // check high root
        positive = _mm256_cmp_ps(quadratic_res.r2, ZERO_PS, _CMP_GT_OQ);
        new_min = _mm256_and_ps(valid, positive);
        new_min = _mm256_and_ps(new_min, _mm256_cmp_ps(quadratic_res.r2, min_factor, _CMP_LT_OQ));

        min_factor = _mm256_blendv_ps(min_factor, quadratic_res.r2, new_min);
        hit_sphere = _mm256_blendv_epi8(hit_sphere, i_lane, _mm256_castps_si256(new_min));
        forward = _mm256_andnot_ps(new_min, forward);
    }

    // forward will be signaling NAN in case of true with all mantissa bits set
    pb.meta = path_meta_face(pb.meta, _mm256_castps_si256(forward));
    pb.meta = path_meta_sphere(pb.meta, hit_sphere);

    __m256i face = path_meta_face(pb.meta);
    int shit[8];
    _mm256_storeu_si256((__m256i *)shit, face);

    __m256 dx = _mm256_mul_ps(pb.ray_dir_x, min_factor);
    __m256 dy = _mm256_mul_ps(pb.ray_dir_y, min_factor);
    __m256 dz = _mm256_mul_ps(pb.ray_dir_z, min_factor);

    pb.ray_orig_x = _mm256_add_ps(pb.ray_orig_x, dx);
    pb.ray_orig_y = _mm256_add_ps(pb.ray_orig_y, dy);
    pb.ray_orig_z = _mm256_add_ps(pb.ray_orig_z, dz);
}

static void direct_light_batch() {
    // get direction of light ray

    for (i32 ls = 0; ls < n_lights; ++ls) {
        __m256i meta = pb.meta;
        __m256i sphere = path_meta_sphere(meta);

        __m256 lx = _mm256_set1_ps(lights[ls].x());
        __m256 ly = _mm256_set1_ps(lights[ls].y());
        __m256 lz = _mm256_set1_ps(lights[ls].z());

        __m256 lvx = _mm256_sub_ps(lx, pb.ray_orig_x);
        __m256 lvy = _mm256_sub_ps(ly, pb.ray_orig_y);
        __m256 lvz = _mm256_sub_ps(lz, pb.ray_orig_z);


        __m256 len = _mm256_mul_ps(lvx, lvx);
        len = _mm256_add_ps(len, _mm256_mul_ps(lvy, lvy));
        len = _mm256_add_ps(len, _mm256_mul_ps(lvz, lvz));
        len = _mm256_sqrt_ps(len);

        __m256 lvx_norm = _mm256_div_ps(lvx, len);
        __m256 lvy_norm = _mm256_div_ps(lvy, len);
        __m256 lvz_norm = _mm256_div_ps(lvz, len);

        pb.ray_orig_x = lx;
        pb.ray_orig_y = ly;
        pb.ray_orig_z = lz;
        pb.ray_dir_x = lvx_norm;
        pb.ray_dir_y = lvy_norm;
        pb.ray_dir_z = lvz_norm;
        intersect_batch();

        __m256 diffx = _mm256_sub_ps(pb.ray_orig_x, lx);
        __m256 diffy = _mm256_sub_ps(pb.ray_orig_y, ly);
        __m256 diffz = _mm256_sub_ps(pb.ray_orig_z, lz);

        //absolute value
        diffx = _mm256_andnot_ps(NEG_ZERO_PS, diffx);
        diffy = _mm256_andnot_ps(NEG_ZERO_PS, diffy);
        diffz = _mm256_andnot_ps(NEG_ZERO_PS, diffz);

        __m256 small = _mm256_cmp_ps(diffx, EPS_PS, _CMP_LT_OQ);
        small = _mm256_and_ps(small, _mm256_cmp_ps(diffy, EPS_PS, _CMP_LT_OQ));
        small = _mm256_and_ps(small, _mm256_cmp_ps(diffz, EPS_PS, _CMP_LT_OQ));

        __m256i should_light = _mm256_cmpeq_epi32(
            path_meta_sphere(pb.meta), 
            _mm256_set1_epi32(SPHERE_NULL));
        should_light = _mm256_and_si256(should_light, _mm256_castps_si256(small));

        // we have to calculate dot product here again 
        // should cache this somewhere it's expensive
        __m256 sphere_x = _mm256_i32gather_ps(sphere_center_x, sphere, LANE_WIDTH);
        __m256 sphere_y = _mm256_i32gather_ps(sphere_center_y, sphere, LANE_WIDTH);
        __m256 sphere_z = _mm256_i32gather_ps(sphere_center_z, sphere, LANE_WIDTH);
        __m256 sphere_r = _mm256_i32gather_ps(sphere_radius, sphere, LANE_WIDTH);

        __m256 normal_x = _mm256_sub_ps(lx, sphere_x);
        __m256 normal_y = _mm256_sub_ps(ly, sphere_y);
        __m256 normal_z = _mm256_sub_ps(lz, sphere_z);

        normal_x = _mm256_div_ps(normal_x, sphere_r);
        normal_y = _mm256_div_ps(normal_y, sphere_r);
        normal_z = _mm256_div_ps(normal_z, sphere_r);

        __m256 light_factor = _mm256_mul_ps(normal_x, lvx);
        light_factor = _mm256_add_ps(light_factor, _mm256_mul_ps(normal_y, lvy));
        light_factor = _mm256_add_ps(light_factor, _mm256_mul_ps(normal_z, lvz));
        light_factor = _mm256_blendv_ps(ZERO_PS, light_factor, _mm256_castsi256_ps(should_light));

        pb.color_r = _mm256_mul_ps(pb.color_r, light_factor);
        pb.color_g = _mm256_mul_ps(pb.color_g, light_factor);
        pb.color_b = _mm256_mul_ps(pb.color_b, light_factor);
    }
}

static void lambertian_kernel() 
{
    alignas(32) static const f32 table_x[64] = {
        0.213f, 0.517f, 0.871f, 0.342f, 0.642f, 0.118f, 0.773f, 0.963f,
        0.401f, 0.689f, 0.245f, 0.834f, 0.512f, 0.076f, 0.921f, 0.387f,
        0.654f, 0.291f, 0.745f, 0.159f, 0.567f, 0.823f, 0.438f, 0.709f,
        0.132f, 0.876f, 0.521f, 0.384f, 0.758f, 0.215f, 0.637f, 0.492f,
        0.781f, 0.314f, 0.628f, 0.451f, 0.069f, 0.843f, 0.356f, 0.934f,
        0.298f, 0.711f, 0.547f, 0.879f, 0.173f, 0.625f, 0.498f, 0.801f,
        0.364f, 0.739f, 0.281f, 0.616f, 0.891f, 0.427f, 0.752f, 0.104f,
        0.569f, 0.837f, 0.224f, 0.692f, 0.378f, 0.815f, 0.541f, 0.658f,
    };
    alignas(32) static const f32 table_y[64] = {
        0.421f, 0.771f, 0.193f, 0.584f, 0.922f, 0.337f, 0.648f, 0.884f,
        0.512f, 0.256f, 0.745f, 0.623f, 0.389f, 0.801f, 0.167f, 0.734f,
        0.451f, 0.867f, 0.312f, 0.678f, 0.524f, 0.091f, 0.756f, 0.413f,
        0.685f, 0.398f, 0.562f, 0.819f, 0.237f, 0.744f, 0.601f, 0.368f,
        0.729f, 0.184f, 0.643f, 0.512f, 0.856f, 0.307f, 0.769f, 0.124f,
        0.634f, 0.478f, 0.821f, 0.297f, 0.715f, 0.351f, 0.563f, 0.889f,
        0.274f, 0.687f, 0.419f, 0.738f, 0.503f, 0.812f, 0.165f, 0.654f,
        0.398f, 0.761f, 0.307f, 0.521f, 0.834f, 0.241f, 0.691f, 0.579f,
    };
    alignas(32) static const f32 table_z[64] = {
        0.308f, 0.662f, 0.124f, 0.537f, 0.789f, 0.241f, 0.603f, 0.918f,
        0.367f, 0.721f, 0.489f, 0.634f, 0.156f, 0.892f, 0.312f, 0.645f,
        0.534f, 0.178f, 0.756f, 0.391f, 0.687f, 0.204f, 0.823f, 0.512f,
        0.261f, 0.719f, 0.456f, 0.641f, 0.894f, 0.328f, 0.763f, 0.597f,
        0.418f, 0.652f, 0.284f, 0.739f, 0.521f, 0.876f, 0.147f, 0.695f,
        0.356f, 0.712f, 0.189f, 0.564f, 0.436f, 0.829f, 0.628f, 0.271f,
        0.743f, 0.367f, 0.612f, 0.289f, 0.865f, 0.147f, 0.709f, 0.523f,
        0.601f, 0.264f, 0.742f, 0.418f, 0.678f, 0.512f, 0.389f, 0.834f,
    };

    for (u32 off = 0; off < ibo->n; off += N_LANES) {
        const i32 active = static_cast<i32>(std::min<u32>(N_LANES, ibo->n - off));

        #warning "may overflow buffer"
        __m256i idxv = _mm256_loadu_si256(reinterpret_cast<__m256i *>(&ibo->idx[off]));

        __m256 ox = _mm256_i32gather_ps(s.ray_orig_x, idxv, LANE_WIDTH);
        __m256 oy = _mm256_i32gather_ps(s.ray_orig_y, idxv, LANE_WIDTH);
        __m256 oz = _mm256_i32gather_ps(s.ray_orig_z, idxv, LANE_WIDTH);
        __m256i meta = _mm256_i32gather_epi32(reinterpret_cast<i32 *>(s.meta), idxv, LANE_WIDTH);
        __m256i sphere_id = path_meta_sphere(meta);

        __m256 cx = _mm256_i32gather_ps(sphere_center_x, sphere_id, LANE_WIDTH);
        __m256 cy = _mm256_i32gather_ps(sphere_center_y, sphere_id, LANE_WIDTH);
        __m256 cz = _mm256_i32gather_ps(sphere_center_z, sphere_id, LANE_WIDTH);
        __m256 r = _mm256_i32gather_ps(sphere_radius, sphere_id, LANE_WIDTH);
        __m256 ar = _mm256_i32gather_ps(sphere_albedo_r, sphere_id, LANE_WIDTH);
        __m256 ag = _mm256_i32gather_ps(sphere_albedo_g, sphere_id, LANE_WIDTH);
        __m256 ab = _mm256_i32gather_ps(sphere_albedo_b, sphere_id, LANE_WIDTH);

        __m256 relx = _mm256_sub_ps(ox, cx);
        __m256 rely = _mm256_sub_ps(oy, cy);
        __m256 relz = _mm256_sub_ps(oz, cz);
        __m256 nrmx = _mm256_div_ps(relx, r);
        __m256 nrmy = _mm256_div_ps(rely, r);
        __m256 nrmz = _mm256_div_ps(relz, r);

        // epsilon correction to make sure ray is outside sphere
        __m256 ox2 = _mm256_add_ps(ox, _mm256_mul_ps(EPS_PS, nrmx));
        __m256 oy2 = _mm256_add_ps(oy, _mm256_mul_ps(EPS_PS, nrmy));
        __m256 oz2 = _mm256_add_ps(oz, _mm256_mul_ps(EPS_PS, nrmz));

        //i32 desired_idx = (165 + 1280 * (720 - 415));
        //for (i32 i = 0; i < N_LANES; ++i) {
        //    if (_mm256_extractvar_epi32(idxv, i) == desired_idx) {
        //        i32 br = 0;
        //    }
        //}

        //__m256i depth = path_meta_depth(meta);
        //__m256i hash = _mm256_add_epi32(
        //    _mm256_mullo_epi32(idxv, _mm256_set1_epi32(1664525)),
        //    _mm256_mullo_epi32(depth, _mm256_set1_epi32(1013904223)));
        //hash = _mm256_xor_si256(hash, _mm256_mullo_epi32(sphere_id, _mm256_set1_epi32(2246822519u)));
        //hash = _mm256_xor_si256(hash, _mm256_srli_epi32(hash, 15));
        //hash = _mm256_mullo_epi32(hash, _mm256_set1_epi32(3266489917u));
        //hash = _mm256_xor_si256(hash, _mm256_srli_epi32(hash, 16));
        //hash = _mm256_and_si256(hash, _mm256_set1_epi32(0x3F));

        //__m256 rx = _mm256_i32gather_ps(table_x, hash, LANE_WIDTH);
        //__m256 ry = _mm256_i32gather_ps(table_y, hash, LANE_WIDTH);
        //__m256 rz = _mm256_i32gather_ps(table_z, hash, LANE_WIDTH);

        //rx = _mm256_set_ps(rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f);
        //ry = _mm256_set_ps(rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f);
        //rz = _mm256_set_ps(rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f);

        __m256 rx = _mm256_rand_ps();
        __m256 ry = _mm256_rand_ps();
        __m256 rz = _mm256_rand_ps();

        // cosine weighted sampling of unit hemisphere
        // generates basis coordinates
        // rx uniform on (-1, 1)
        __m256 rx_norm = _mm256_sub_ps(_mm256_mul_ps(rx, TWO_PS), ONE_PS);

        // ry uniform on chord in hemishpere base
        __m256 ry_norm = _mm256_sub_ps(ONE_PS, _mm256_mul_ps(rx_norm, rx_norm));
        ry_norm = _mm256_sqrt_ps(ry_norm);
        ry_norm = _mm256_mul_ps(ry_norm, _mm256_sub_ps(_mm256_mul_ps(ry, TWO_PS), ONE_PS));

        __m256 rz_norm = _mm256_mul_ps(rx_norm, rx_norm);
        rz_norm = _mm256_add_ps(rz_norm, _mm256_mul_ps(ry_norm, ry_norm));
        rz_norm = _mm256_sub_ps(ONE_PS, rz_norm);
        rz_norm = _mm256_sqrt_ps(rz_norm);

        __m256 len = _mm256_add_ps(
            _mm256_mul_ps(rx_norm, rx_norm), 
            _mm256_add_ps(
                _mm256_mul_ps(ry_norm, ry_norm), 
                _mm256_mul_ps(rz_norm, rz_norm)));

        // branchless orthonormal basis generation
        __m256 dir_x;
        __m256 dir_y;
        __m256 dir_z;
        {
            // copysign(1, z)
            __m256 sign = _mm256_and_ps(nrmz, NEG_ZERO_PS);
            sign = _mm256_or_ps(sign, ONE_PS);

            __m256 a = _mm256_div_ps(NEG_ONE_PS, _mm256_add_ps(sign, nrmz));
            __m256 b = _mm256_mul_ps(_mm256_mul_ps(nrmx, nrmy), a);

            __m256 t1x = _mm256_add_ps(
                ONE_PS, 
                _mm256_mul_ps(_mm256_mul_ps(sign, nrmx), _mm256_mul_ps(nrmx, a)));
            __m256 t1y = _mm256_mul_ps(sign, b);
            __m256 t1z = _mm256_mul_ps(_mm256_sub_ps(ZERO_PS, sign), nrmx);

            __m256 t2x = b;
            __m256 t2y = _mm256_add_ps(sign, _mm256_mul_ps(_mm256_mul_ps(nrmy, nrmy), a));
            __m256 t2z = _mm256_sub_ps(ZERO_PS, nrmy);

            dir_x = _mm256_mul_ps(t1x, rx_norm);
            dir_x = _mm256_add_ps(dir_x, _mm256_mul_ps(t2x, ry_norm));
            dir_x = _mm256_add_ps(dir_x, _mm256_mul_ps(nrmx, rz_norm));

            dir_y = _mm256_mul_ps(t1y, rx_norm);
            dir_y = _mm256_add_ps(dir_y, _mm256_mul_ps(t2y, ry_norm));
            dir_y = _mm256_add_ps(dir_y, _mm256_mul_ps(nrmy, rz_norm));

            dir_z = _mm256_mul_ps(t1z, rx_norm);
            dir_z = _mm256_add_ps(dir_z, _mm256_mul_ps(t2z, ry_norm));
            dir_z = _mm256_add_ps(dir_z, _mm256_mul_ps(nrmz, rz_norm));
        }

        len = _mm256_add_ps(
            _mm256_mul_ps(dir_x, dir_x),
            _mm256_add_ps(
                _mm256_mul_ps(dir_y, dir_y),
                _mm256_mul_ps(dir_z, dir_z)));

        __m256 cr = _mm256_i32gather_ps(s.color_r, idxv, LANE_WIDTH);
        __m256 cg = _mm256_i32gather_ps(s.color_g, idxv, LANE_WIDTH);
        __m256 cb = _mm256_i32gather_ps(s.color_b, idxv, LANE_WIDTH);

        cr = _mm256_mul_ps(cr, ar);
        cg = _mm256_mul_ps(cg, ag);
        cb = _mm256_mul_ps(cb, ab);

        pb.ray_orig_x = ox2;
        pb.ray_orig_y = oy2;
        pb.ray_orig_z = oz2;
        pb.ray_dir_x = dir_x;
        pb.ray_dir_y = dir_y;
        pb.ray_dir_z = dir_z;
        intersect_batch();

        for (i32 lane = 0; lane < active; ++lane) {
            const u32 idx = _mm256_extractvar_epi32(idxv, lane);
            i32 hit_sphere = path_meta_sphere(_mm256_extractvar_epi32(pb.meta, lane));

            s.color_r[idx] = _mm256_extract_ps(cr, lane);
            s.color_g[idx] = _mm256_extract_ps(cg, lane);
            s.color_b[idx] = _mm256_extract_ps(cb, lane);

            if (hit_sphere == SPHERE_NULL)
                continue;

            i32 mat_idx = static_cast<i32>(spheres[hit_sphere].mat.ty);
            index_buffer_t *next_ibo = &next_ibo_batch[mat_idx];
            next_ibo->idx[next_ibo->n++] = idx;

            s.ray_orig_x[idx] = _mm256_extract_ps(pb.ray_orig_x, lane);
            s.ray_orig_y[idx] = _mm256_extract_ps(pb.ray_orig_y, lane);
            s.ray_orig_z[idx] = _mm256_extract_ps(pb.ray_orig_z, lane);
            s.ray_dir_x[idx] = _mm256_extract_ps(dir_x, lane);
            s.ray_dir_y[idx] = _mm256_extract_ps(dir_y, lane);
            s.ray_dir_z[idx] = _mm256_extract_ps(dir_z, lane);
            s.meta[idx] = path_meta_depth(pb.meta[idx], path_meta_depth(pb.meta[idx]) + 1u);
            s.meta[idx] = path_meta_sphere(pb.meta[idx], hit_sphere);
        }
    }
}

static void reflective_kernel() 
{
    alignas(32) static const f32 table[8] = {0.13f, 0.47f, 0.81f, 0.27f, 0.63f, 0.19f, 0.74f, 0.92f};

    for (u32 off = 0; off < ibo->n; off += N_LANES) {
        const i32 active = static_cast<i32>(std::min<u32>(N_LANES, ibo->n - off));

        #warning "may overflow buffer"
        __m256i idxv = _mm256_loadu_si256(reinterpret_cast<__m256i *>(&ibo->idx[off]));

        __m256 ox = _mm256_i32gather_ps(s.ray_orig_x, idxv, LANE_WIDTH);
        __m256 oy = _mm256_i32gather_ps(s.ray_orig_y, idxv, LANE_WIDTH);
        __m256 oz = _mm256_i32gather_ps(s.ray_orig_z, idxv, LANE_WIDTH);
        __m256 dx = _mm256_i32gather_ps(s.ray_dir_x, idxv, LANE_WIDTH);
        __m256 dy = _mm256_i32gather_ps(s.ray_dir_y, idxv, LANE_WIDTH);
        __m256 dz = _mm256_i32gather_ps(s.ray_dir_z, idxv, LANE_WIDTH);
        __m256i meta = _mm256_i32gather_epi32(reinterpret_cast<i32 *>(s.meta), idxv, sizeof(path_meta_t));
        __m256i sphere_id = path_meta_sphere(meta);

        __m256 cx = _mm256_i32gather_ps(sphere_center_x, sphere_id, LANE_WIDTH);
        __m256 cy = _mm256_i32gather_ps(sphere_center_y, sphere_id, LANE_WIDTH);
        __m256 cz = _mm256_i32gather_ps(sphere_center_z, sphere_id, LANE_WIDTH);
        __m256 r = _mm256_i32gather_ps(sphere_radius, sphere_id, LANE_WIDTH);

        __m256 rel_x = _mm256_sub_ps(ox, cx);
        __m256 rel_y = _mm256_sub_ps(oy, cy);
        __m256 rel_z = _mm256_sub_ps(oz, cz);
        __m256 norm_x = _mm256_div_ps(rel_x, r);
        __m256 norm_y = _mm256_div_ps(rel_y, r);
        __m256 norm_z = _mm256_div_ps(rel_z, r);

        __m256 dot = _mm256_mul_ps(dx, norm_x);
        dot = _mm256_add_ps(dot, _mm256_mul_ps(dy, norm_y));
        dot = _mm256_add_ps(dot, _mm256_mul_ps(dz, norm_z));

        __m256 dir_x = _mm256_sub_ps(dx, _mm256_mul_ps(TWO_PS, _mm256_mul_ps(dot, norm_x)));
        __m256 dir_y = _mm256_sub_ps(dy, _mm256_mul_ps(TWO_PS, _mm256_mul_ps(dot, norm_y)));
        __m256 dir_z = _mm256_sub_ps(dz, _mm256_mul_ps(TWO_PS, _mm256_mul_ps(dot, norm_z)));

        __m256 ox2 = _mm256_add_ps(ox, _mm256_mul_ps(EPS_PS, norm_x));
        __m256 oy2 = _mm256_add_ps(oy, _mm256_mul_ps(EPS_PS, norm_y));
        __m256 oz2 = _mm256_add_ps(oz, _mm256_mul_ps(EPS_PS, norm_z));

        pb.ray_orig_x = ox2;
        pb.ray_orig_y = oy2;
        pb.ray_orig_z = oz2;
        pb.ray_dir_x = dir_x;
        pb.ray_dir_y = dir_y;
        pb.ray_dir_z = dir_z;
        intersect_batch();

        for (i32 lane = 0; lane < active; ++lane) {
            const u32 idx = _mm256_extractvar_epi32(idxv, lane);
            i32 hit_sphere = path_meta_sphere(_mm256_extractvar_epi32(pb.meta, lane));

            if (hit_sphere == SPHERE_NULL)
                continue;

            i32 mat_idx = static_cast<i32>(spheres[hit_sphere].mat.ty);
            index_buffer_t *next_ibo = &next_ibo_batch[mat_idx];
            next_ibo->idx[next_ibo->n++] = idx;

            s.ray_orig_x[idx] = _mm256_extract_ps(pb.ray_orig_x, lane);
            s.ray_orig_y[idx] = _mm256_extract_ps(pb.ray_orig_y, lane);
            s.ray_orig_z[idx] = _mm256_extract_ps(pb.ray_orig_z, lane);
            s.ray_dir_x[idx] = _mm256_extract_ps(dir_x, lane);
            s.ray_dir_y[idx] = _mm256_extract_ps(dir_y, lane);
            s.ray_dir_z[idx] = _mm256_extract_ps(dir_z, lane);
            s.meta[idx] = path_meta_depth(pb.meta[idx], path_meta_depth(pb.meta[idx]) + 1u);
        }
    }
}

static void refractive_kernel() 
{
    alignas(32) static const f32 table[8] = {0.11f, 0.45f, 0.83f, 0.28f, 0.61f, 0.15f, 0.76f, 0.94f};

    for (u32 off = 0; off < ibo->n; off += N_LANES) {
        const i32 active = static_cast<i32>(std::min<u32>(N_LANES, ibo->n - off));

        #warning "may overflow buffer"
        __m256i idxv = _mm256_loadu_si256(reinterpret_cast<__m256i *>(&ibo->idx[off]));

        __m256 ox = _mm256_i32gather_ps(s.ray_orig_x, idxv, LANE_WIDTH);
        __m256 oy = _mm256_i32gather_ps(s.ray_orig_y, idxv, LANE_WIDTH);
        __m256 oz = _mm256_i32gather_ps(s.ray_orig_z, idxv, LANE_WIDTH);
        __m256 dx = _mm256_i32gather_ps(s.ray_dir_x, idxv, LANE_WIDTH);
        __m256 dy = _mm256_i32gather_ps(s.ray_dir_y, idxv, LANE_WIDTH);
        __m256 dz = _mm256_i32gather_ps(s.ray_dir_z, idxv, LANE_WIDTH);
        __m256i meta = _mm256_i32gather_epi32(reinterpret_cast<i32 *>(s.meta), idxv, sizeof(path_meta_t));
        __m256i sphere_id = path_meta_sphere(meta);
        __m256i face = path_meta_face(meta);

        __m256 cx = _mm256_i32gather_ps(sphere_center_x, sphere_id, LANE_WIDTH);
        __m256 cy = _mm256_i32gather_ps(sphere_center_y, sphere_id, LANE_WIDTH);
        __m256 cz = _mm256_i32gather_ps(sphere_center_z, sphere_id, LANE_WIDTH);
        __m256 r = _mm256_i32gather_ps(sphere_radius, sphere_id, LANE_WIDTH);
        __m256 ior = _mm256_i32gather_ps(sphere_ior, sphere_id, LANE_WIDTH);
        __m256 r0 = _mm256_i32gather_ps(sphere_reflectance, sphere_id, LANE_WIDTH);

        __m256 rel_x = _mm256_sub_ps(ox, cx);
        __m256 rel_y = _mm256_sub_ps(oy, cy);
        __m256 rel_z = _mm256_sub_ps(oz, cz);
        __m256 norm_x = _mm256_div_ps(rel_x, r);
        __m256 norm_y = _mm256_div_ps(rel_y, r);
        __m256 norm_z = _mm256_div_ps(rel_z, r);

        __m256 orientation = _mm256_sub_ps(
            _mm256_mul_ps(_mm256_cvtepi32_ps(face), TWO_PS), 
            ONE_PS);
        norm_x = _mm256_mul_ps(orientation, norm_x);
        norm_y = _mm256_mul_ps(orientation, norm_y);
        norm_z = _mm256_mul_ps(orientation, norm_z);

        //i32 desired_idx = (165 + 1280 * (720 - 415));
        //for (i32 i = 0; i < N_LANES; ++i) {
        //    if (_mm256_extractvar_epi32(idxv, i) == desired_idx) {
        //        i32 br = 0;
        //    }
        //}

        // make sure ray crosses sphere boundary to avoid self intersection
        ox = _mm256_sub_ps(ox, _mm256_mul_ps(EPS_PS, norm_x));
        oy = _mm256_sub_ps(oy, _mm256_mul_ps(EPS_PS, norm_y));
        oz = _mm256_sub_ps(oz, _mm256_mul_ps(EPS_PS, norm_z));

        // snells law
        __m256 cos_i = _mm256_mul_ps(norm_x, dx);
        cos_i = _mm256_add_ps(cos_i, _mm256_mul_ps(norm_y, dy));
        cos_i = _mm256_add_ps(cos_i, _mm256_mul_ps(norm_z, dz));
        cos_i = _mm256_mul_ps(cos_i, NEG_ONE_PS);
        cos_i = _mm256_max_ps(ZERO_PS, _mm256_min_ps(ONE_PS, cos_i));

        // float comparisons check the sign bit our control uses lsb
        // so we must map to float with corresponding sign bit
        __m256 mask = _mm256_cvtepi32_ps(face);
        mask = _mm256_sub_ps(_mm256_set1_ps(0.5f), mask);
        __m256 n1n2 = _mm256_blendv_ps(_mm256_div_ps(ONE_PS, ior), ior, mask);
        __m256 sin_r_2 = _mm256_sub_ps(ONE_PS, _mm256_mul_ps(cos_i, cos_i));
        sin_r_2 = _mm256_mul_ps(sin_r_2, _mm256_mul_ps(n1n2, n1n2));

        __m256 tir = _mm256_cmp_ps(sin_r_2, ONE_PS, _CMP_GT_OQ);

        //sin_r_2 = _mm256_min_ps(sin_r_2, ONE_PS);
        __m256 cos_r = _mm256_sqrt_ps(_mm256_sub_ps(ONE_PS, sin_r_2));
        __m256 norm_f = _mm256_sub_ps(_mm256_mul_ps(n1n2, cos_i), cos_r);
        __m256 refr_x = _mm256_add_ps(_mm256_mul_ps(dx, n1n2), _mm256_mul_ps(norm_x, norm_f));
        __m256 refr_y = _mm256_add_ps(_mm256_mul_ps(dy, n1n2), _mm256_mul_ps(norm_y, norm_f));
        __m256 refr_z = _mm256_add_ps(_mm256_mul_ps(dz, n1n2), _mm256_mul_ps(norm_z, norm_f));

        __m256 ref_x = _mm256_add_ps(dx, _mm256_mul_ps(TWO_PS, _mm256_mul_ps(cos_i, norm_x)));
        __m256 ref_y = _mm256_add_ps(dy, _mm256_mul_ps(TWO_PS, _mm256_mul_ps(cos_i, norm_y)));
        __m256 ref_z = _mm256_add_ps(dz, _mm256_mul_ps(TWO_PS, _mm256_mul_ps(cos_i, norm_z)));

        __m256 one_minus_c = _mm256_sub_ps(ONE_PS, cos_i);
        __m256 p2 = _mm256_mul_ps(one_minus_c, one_minus_c);
        __m256 p5 = _mm256_mul_ps(_mm256_mul_ps(p2, p2), one_minus_c);
        __m256 fresnel = _mm256_add_ps(r0, _mm256_mul_ps(_mm256_sub_ps(ONE_PS, r0), p5));

        __m256 choose_reflection = _mm256_or_ps(tir,
            _mm256_cmp_ps(fresnel, _mm256_set_ps(rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f, rand_f), _CMP_GT_OQ));

        __m256 dir_x = _mm256_blendv_ps(refr_x, ref_x, choose_reflection);
        __m256 dir_y = _mm256_blendv_ps(refr_y, ref_y, choose_reflection);
        __m256 dir_z = _mm256_blendv_ps(refr_z, ref_z, choose_reflection);

        __m256 out_len = _mm256_mul_ps(dir_x, dir_x);
        out_len = _mm256_add_ps(out_len, _mm256_mul_ps(dir_y, dir_y));
        out_len = _mm256_add_ps(out_len, _mm256_mul_ps(dir_z, dir_z));
        out_len = _mm256_sqrt_ps(out_len);

        dir_x = _mm256_div_ps(dir_x, out_len);
        dir_y = _mm256_div_ps(dir_y, out_len);
        dir_z = _mm256_div_ps(dir_z, out_len);

        pb.ray_orig_x = ox;
        pb.ray_orig_y = oy;
        pb.ray_orig_z = oz;
        pb.ray_dir_x = dir_x;
        pb.ray_dir_y = dir_y;
        pb.ray_dir_z = dir_z;
        intersect_batch();

        // Wish we could scatter instead of using scalar stores
        for (i32 lane = 0; lane < active; ++lane) {
            const u32 idx = _mm256_extractvar_epi32(idxv, lane);
            i32 hit_sphere = path_meta_sphere(_mm256_extractvar_epi32(pb.meta, lane));

            if (hit_sphere == SPHERE_NULL)
                continue;

            i32 mat_idx = static_cast<i32>(spheres[hit_sphere].mat.ty);
            index_buffer_t *next_ibo = &next_ibo_batch[mat_idx];
            next_ibo->idx[next_ibo->n++] = idx;

            s.ray_dir_x[idx] = _mm256_extract_ps(dir_x, lane);
            s.ray_dir_y[idx] = _mm256_extract_ps(dir_y, lane);
            s.ray_dir_z[idx] = _mm256_extract_ps(dir_z, lane);
            s.ray_orig_x[idx] = _mm256_extract_ps(pb.ray_orig_x, lane);
            s.ray_orig_y[idx] = _mm256_extract_ps(pb.ray_orig_y, lane);
            s.ray_orig_z[idx] = _mm256_extract_ps(pb.ray_orig_z, lane);
            s.meta[idx] = _mm256_extractvar_epi32(pb.meta, lane);
            //s.meta[idx] = path_meta_depth(pb.meta[idx], path_meta_depth(pb.meta[idx]) + 1u);
        }
    }
}

static void dispatch_shading_kernels() 
{
    for (i32 material_idx = 0; material_idx < MAT_LEN; ++material_idx) {
        ibo = &ibo_batch[material_idx];
        if (ibo->n == 0)
            continue;

        switch (static_cast<material_type>(material_idx)) {
            case material_type::lambertian:
                lambertian_kernel();
                break;
            case material_type::reflective:
                reflective_kernel();
                break;
            case material_type::refractive:
                refractive_kernel();
                break;
            default:
                break;
        }

        ibo->n = 0;
    }
}

void store_colors() {

}

void rt_draw_frame(const scene_t &scene)
{
    rt_set_scene(scene);

    f32 dx = static_cast<f32>(v_viewport_width) / v_width;
    f32 dy = static_cast<f32>(v_viewport_height) / v_height;

    for (auto y = 0; y < v_height; ++y) {
        i32 y_t = v_height - y - 1;

        #ifdef DEBUG_LOG
        std::cout << "\r                    \r"
                  << static_cast<f32>(y) / v_height
                  << std::flush;
        #endif

        i32 remaining_lanes = v_width;
        i32 x = 0;
        i32 idx = y * v_width + x;

        while (remaining_lanes >= N_LANES) {
            __m256 dx = PROG_PS(static_cast<f32>(x));
            dx = _mm256_div_ps(dx, _mm256_set1_ps(v_width));
            dx = _mm256_sub_ps(dx, _mm256_set1_ps(0.5f));
            dx = _mm256_mul_ps(dx, _mm256_set1_ps(v_viewport_width));

            __m256 dy = _mm256_set1_ps(y);
            dy = _mm256_div_ps(dy, _mm256_set1_ps(v_height));
            dy = _mm256_sub_ps(dy, _mm256_set1_ps(0.5f));
            dy = _mm256_mul_ps(dy, _mm256_set1_ps(v_viewport_height));

            // multisample transform
            dx = _mm256_add_ps(dx, _mm256_set1_ps(off_x));
            dy = _mm256_add_ps(dy, _mm256_set1_ps(off_y));

            __m256 dz = _mm256_set1_ps(v_near);
            
            __m256 len = _mm256_mul_ps(dx, dx);
            len = _mm256_add_ps(len, _mm256_mul_ps(dy, dy));
            len = _mm256_add_ps(len, _mm256_mul_ps(dz, dz));
            len = _mm256_sqrt_ps(len);

            dx = _mm256_div_ps(dx, len);
            dy = _mm256_div_ps(dy, len);
            dz = _mm256_div_ps(dz, len);

            _mm256_store_ps(s.ray_dir_x + idx, dx);
            _mm256_store_ps(s.ray_dir_y + idx, dy);
            _mm256_store_ps(s.ray_dir_z + idx, dz);

            _mm256_store_ps(s.ray_orig_x + idx, ZERO_PS);
            _mm256_store_ps(s.ray_orig_y + idx, ZERO_PS);
            _mm256_store_ps(s.ray_orig_z + idx, ZERO_PS);

            _mm256_store_ps(s.color_r + idx, ONE_PS);
            _mm256_store_ps(s.color_g + idx, ONE_PS);
            _mm256_store_ps(s.color_b + idx, ONE_PS);

            __m256i meta = path_meta_depth(ZERO_EPI32, ONE_EPI32);
            _mm256_store_si256(reinterpret_cast<__m256i *>(s.meta + idx), meta);

            pb.ray_orig_x = ZERO_PS;
            pb.ray_orig_y = ZERO_PS;
            pb.ray_orig_z = ZERO_PS;
            pb.ray_dir_x = dx;
            pb.ray_dir_y = dy;
            pb.ray_dir_z = dz;
            pb.color_r = ONE_PS;
            pb.color_g = ONE_PS;
            pb.color_b = ONE_PS;
            pb.meta = meta;

            intersect_batch();

            __m256i sphere = path_meta_sphere(pb.meta);

            // Write the updated metadata to state 
            _mm256_store_ps(s.ray_orig_x + idx, pb.ray_orig_x);
            _mm256_store_ps(s.ray_orig_y + idx, pb.ray_orig_y);
            _mm256_store_ps(s.ray_orig_z + idx, pb.ray_orig_z);
            _mm256_store_si256(reinterpret_cast<__m256i *>(s.meta + idx), pb.meta);

            for (auto i = 0; i < N_LANES; ++i) {
                i32 s = _mm256_extractvar_epi32(sphere, i);
                if (s == SPHERE_NULL)
                    continue;
                    
                i32 ty = static_cast<i32>(spheres[s].mat.ty);
                ibo_batch[ty].idx[ibo_batch[ty].n++] = idx + i;
            }

            remaining_lanes -= N_LANES;
            x += N_LANES;
            idx += N_LANES;
        }

        for (; x < v_width; ++x) {
            f32 vx = v_viewport_width *
                     (static_cast<float>(x) / static_cast<float>(v_width) - 0.5f);
            f32 vy = v_viewport_height *
                     (static_cast<float>(y) / static_cast<float>(v_height) - 0.5f);
            f32 vz = v_near;

            vx += off_x;
            vy += off_y;

            ray look_ray{vec3{0.f, 0.f, 0.f}, vec3{vx, vy, vz}};

            s.ray_orig_x[idx] = 0.f;
            s.ray_orig_y[idx] = 0.f;
            s.ray_orig_z[idx] = 0.f;

            s.ray_dir_x[idx] = look_ray.dir.x();
            s.ray_dir_y[idx] = look_ray.dir.y();
            s.ray_dir_z[idx] = look_ray.dir.z();

            s.color_r[idx] = 1.f;
            s.color_g[idx] = 1.f;
            s.color_b[idx] = 1.f;

            s.meta[idx] = path_meta_sphere(path_meta_depth(0, 1), 1);

            f32 min_factor = SPHERE_FACTOR_MAX;
            i32 hit_sphere = SPHERE_NULL;
            bool face = false;

            for (i32 s = 0; s < sphere_top; ++s) {
                if (s == SPHERE_NULL)
                    continue;

                quadratic_result qr = spheres[s].intersection_ray(look_ray);

                if (qr.nan)
                    continue;

                if (qr.low > 0.f && qr.low < min_factor) {
                    min_factor = qr.low;
                    hit_sphere = s;
                    face = true;
                } else if (qr.high > 0.f && qr.high < min_factor) {
                    min_factor = qr.high;
                    hit_sphere = s;
                    face = false;
                }
            }

            if (hit_sphere != SPHERE_NULL) {
                const vec3 hit = look_ray.orig + look_ray.dir * min_factor;
                s.ray_orig_x[idx] = hit.x();
                s.ray_orig_y[idx] = hit.y();
                s.ray_orig_z[idx] = hit.z();
                s.meta[idx] = path_meta_sphere(s.meta[idx], static_cast<u32>(hit_sphere));
                s.meta[idx] = path_meta_face(s.meta[idx], static_cast<u32>(face));
                s.meta[idx] = path_meta_depth(s.meta[idx], path_meta_depth(s.meta[idx]) + 1u);

                i32 mat_idx = static_cast<i32>(spheres[hit_sphere].mat.ty);
                ibo[mat_idx].idx[ibo[mat_idx].n++] = idx;
            }
        }
    }

    for (i32 wavefront_stage = 0; wavefront_stage < 10; ++wavefront_stage) {
        if (ibo_batch[0].n == 0 && ibo_batch[1].n == 0 && ibo_batch[2].n == 0) {
            break;
        }

        dispatch_shading_kernels();

        for (i32 material_idx = 0; material_idx < MAT_LEN; ++material_idx) {
            ibo_batch[material_idx].n = 0;
        }

        index_buffer_t *temp = ibo_batch;
        ibo_batch = next_ibo_batch;
        next_ibo_batch = temp;
    }

    for (i32 y = 0, y_t = v_height - 1; y < v_height; ++y, --y_t) {
        for (i32 x = 0; x < v_width; ++x) {
            const i32 idx = y * v_width + x;

            const f32 rr = std::clamp(s.color_r[idx], 0.f, 1.f);
            const f32 gg = std::clamp(s.color_g[idx], 0.f, 1.f);
            const f32 bb = std::clamp(s.color_b[idx], 0.f, 1.f);

            color3 color{rr, gg, bb};
            color *= background_color;

            //gamma transform sort of
            color.e[0] = std::sqrtf(color[0]);
            color.e[1] = std::sqrtf(color[1]);
            color.e[2] = std::sqrtf(color[2]);

            //multisample transform
            color /= static_cast<float>(n_samples);

            ctx->add_pixel(x, y_t, color);
        }
    }

    #ifdef DEBUG_LOG
    std::cout<<'\n';
    #endif
}

void rt_multisample_draw(const scene_t &s, int n) 
{
    f32 dx = static_cast<float>(v_viewport_width) / static_cast<float>(v_width);
    f32 dy = static_cast<float>(v_viewport_height) / static_cast<float>(v_height);

    offset_table o(n, dx, dy);

    n_samples = n;

    for (i32 sample = 0; sample < n; ++sample) {
        off_x = o[sample];
        off_y = o[sample + 1];

        rt_draw_frame(s);
    }

    off_x = 0.f;
    off_y = 0.f;
    n_samples = 1;
}

void rt_init(Ctx &c) 
{
    ctx = &c;
}

void rt_set_scene(const scene_t &scene) 
{
    std::memcpy(spheres + 1, scene.spheres, scene.n_spheres * sizeof(sphere));
    sphere_top = scene.n_spheres + 1;
    sync_sphere_tables();
}
