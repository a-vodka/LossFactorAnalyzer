#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

// ---------- Utilities ----------
inline std::vector<float> linspace(float start, float end, size_t num) {
    std::vector<float> r(num);
    if (num == 1) { r[0] = start; return r; }
    float step = (end - start) / static_cast<float>(num - 1);
    for (size_t i = 0; i < num; ++i) r[i] = start + step * static_cast<float>(i);
    return r;
}

inline float mean(const std::vector<float>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

inline float stddev(const std::vector<float>& v, float vmean=-1.0) {
    if (v.empty()) return 0.0;
    if (vmean < 0) vmean = mean(v);
    float s = 0.0;
    for (float x : v) s += (x - vmean)*(x - vmean);
    return std::sqrt(s / v.size());
}

// ---------- Model ----------
inline float skewed_lorentzian(float f, float A0, float f0, float eta, float alpha, float offset) {
    float df_norm = (f - f0) / f0;
    float denom = 1.0 + (df_norm * df_norm) / (eta * eta);
    return A0 / denom * (1.0 + alpha * df_norm) + offset;
}

// ---------- SSE ----------
inline float sse(const std::vector<float>& x, const std::vector<float>& y,
                  float A0, float f0, float eta, float alpha, float offset) {
    float sum = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        float d = skewed_lorentzian(x[i], A0, f0, eta, alpha, offset) - y[i];
        sum += d * d;
    }
    return sum;
}

// ---------- Simple coordinate-search optimizer (very small-data friendly) ----------
inline void fit_skewed_lorentzian_basic(const std::vector<float>& x, const std::vector<float>& y,
                                        float &A0, float &f0, float &eta, float &alpha, float &offset,
                                        int max_iters = 1000) {
    // initial step sizes (relative)
    float stepA0 = std::max(1e-6, std::abs(A0) * 0.1);
    float stepf0 = std::max(1e-6, std::abs(f0) * 0.05);
    float stepEta = std::max(1e-6, std::abs(eta) * 0.1);
    float stepAlpha = std::max(1e-6, 0.1);
    float stepOff = std::max(1e-6, (std::abs(offset) > 0 ? std::abs(offset) * 0.1 : 0.1));

    float bestErr = sse(x, y, A0, f0, eta, alpha, offset);

    for (int iter = 0; iter < max_iters; ++iter) {
        bool improved = false;
        float params[5] = {A0, f0, eta, alpha, offset};
        float steps[5]  = {stepA0, stepf0, stepEta, stepAlpha, stepOff};

        for (int p = 0; p < 5; ++p) {
            for (int dir = -1; dir <= 1; dir += 2) {
                float test[5] = {params[0], params[1], params[2], params[3], params[4]};
                test[p] += dir * steps[p];

                // keep physical constraints: eta > 0, f0 > 0
                if (test[2] <= 0.0) continue;
                if (test[1] <= 0.0) continue;

                float err = sse(x, y, test[0], test[1], test[2], test[3], test[4]);
                if (err < bestErr) {
                    bestErr = err;
                    A0 = test[0]; f0 = test[1]; eta = test[2]; alpha = test[3]; offset = test[4];
                    improved = true;
                }
            }
        }
        if (!improved) {
            // reduce steps
            stepA0 *= 0.5; stepf0 *= 0.5; stepEta *= 0.5; stepAlpha *= 0.5; stepOff *= 0.5;
            if (stepA0 < 1e-9 && stepf0 < 1e-9 && stepEta < 1e-9 && stepAlpha < 1e-9 && stepOff < 1e-9) break;
        }
    }
}
