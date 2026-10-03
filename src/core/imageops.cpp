#include "imageops.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace wm {

namespace {

const double kPi = 3.14159265358979323846;

double Sinc(double x) {
    if (x == 0.0) return 1.0;
    double px = kPi * x;
    return std::sin(px) / px;
}
double Lanczos3(double x) {
    x = std::fabs(x);
    return x < 3.0 ? Sinc(x) * Sinc(x / 3.0) : 0.0;
}

struct Contrib {
    int start = 0;
    std::vector<float> w;
};

std::vector<Contrib> BuildContribs(int srcN, int dstN) {
    std::vector<Contrib> out(dstN);
    double scale = (double)srcN / dstN;
    double fscale = std::max(1.0, scale);
    double support = 3.0 * fscale;
    for (int i = 0; i < dstN; ++i) {
        double center = (i + 0.5) * scale;
        int lo = (int)std::floor(center - support + 0.5);
        int hi = (int)std::floor(center + support + 0.5);
        lo = std::max(lo, 0);
        hi = std::min(hi, srcN);
        Contrib& c = out[i];
        c.start = lo;
        double sum = 0;
        for (int j = lo; j < hi; ++j) {
            double w = Lanczos3((j + 0.5 - center) / fscale);
            c.w.push_back((float)w);
            sum += w;
        }
        if (sum != 0) for (auto& w : c.w) w = (float)(w / sum);
        if (c.w.empty()) { c.start = std::min(std::max((int)center, 0), srcN - 1); c.w.push_back(1.0f); }
    }
    return out;
}

}  // namespace

FImage Resize(const FImage& src, int newW, int newH) {
    if (src.empty() || newW <= 0 || newH <= 0) return FImage();
    if (newW == src.w && newH == src.h) return src;
    auto cx = BuildContribs(src.w, newW);
    auto cy = BuildContribs(src.h, newH);

    FImage tmp(newW, src.h);
    for (int y = 0; y < src.h; ++y) {
        for (int x = 0; x < newW; ++x) {
            const Contrib& c = cx[x];
            float acc[4] = {0, 0, 0, 0};
            for (size_t k = 0; k < c.w.size(); ++k) {
                const float* p = src.px(c.start + (int)k, y);
                for (int ch = 0; ch < 4; ++ch) acc[ch] += p[ch] * c.w[k];
            }
            float* o = tmp.px(x, y);
            for (int ch = 0; ch < 4; ++ch) o[ch] = acc[ch];
        }
    }
    FImage dst(newW, newH);
    for (int y = 0; y < newH; ++y) {
        const Contrib& c = cy[y];
        for (int x = 0; x < newW; ++x) {
            float acc[4] = {0, 0, 0, 0};
            for (size_t k = 0; k < c.w.size(); ++k) {
                const float* p = tmp.px(x, c.start + (int)k);
                for (int ch = 0; ch < 4; ++ch) acc[ch] += p[ch] * c.w[k];
            }
            float* o = dst.px(x, y);
            // Lanczos ma ujemne listki: przytnij do poprawnej premultiplikowanej alfy.
            float a = std::min(std::max(acc[3], 0.0f), 1.0f);
            o[3] = a;
            for (int ch = 0; ch < 3; ++ch) o[ch] = std::min(std::max(acc[ch], 0.0f), a);
        }
    }
    return dst;
}

void FlipHorizontal(FImage& img) {
    for (int y = 0; y < img.h; ++y)
        for (int x = 0; x < img.w / 2; ++x) {
            float* a = img.px(x, y);
            float* b = img.px(img.w - 1 - x, y);
            for (int c = 0; c < 4; ++c) std::swap(a[c], b[c]);
        }
}

void FlipVertical(FImage& img) {
    for (int y = 0; y < img.h / 2; ++y)
        for (int x = 0; x < img.w; ++x) {
            float* a = img.px(x, y);
            float* b = img.px(x, img.h - 1 - y);
            for (int c = 0; c < 4; ++c) std::swap(a[c], b[c]);
        }
}

namespace {

double CubicWeight(double t) {  // Catmull-Rom
    t = std::fabs(t);
    if (t < 1.0) return 1.5 * t * t * t - 2.5 * t * t + 1.0;
    if (t < 2.0) return -0.5 * t * t * t + 2.5 * t * t - 4.0 * t + 2.0;
    return 0.0;
}

FImage Rotate90(const FImage& src, int quarter) {  // quarter: 1,2,3 (zgodnie z zegarem)
    if (quarter == 2) {
        FImage o(src.w, src.h);
        for (int y = 0; y < src.h; ++y)
            for (int x = 0; x < src.w; ++x) {
                const float* p = src.px(x, y);
                float* q = o.px(src.w - 1 - x, src.h - 1 - y);
                for (int c = 0; c < 4; ++c) q[c] = p[c];
            }
        return o;
    }
    FImage o(src.h, src.w);
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            const float* p = src.px(x, y);
            int nx, ny;
            if (quarter == 1) { nx = src.h - 1 - y; ny = x; }
            else { nx = y; ny = src.w - 1 - x; }
            float* q = o.px(nx, ny);
            for (int c = 0; c < 4; ++c) q[c] = p[c];
        }
    return o;
}

}  // namespace

FImage Rotate(const FImage& src, double degrees) {
    if (src.empty()) return FImage();
    double d = std::fmod(degrees, 360.0);
    if (d < 0) d += 360.0;
    if (d < 1e-9 || d > 360.0 - 1e-9) return src;
    if (std::fabs(d - 90.0) < 1e-9) return Rotate90(src, 1);
    if (std::fabs(d - 180.0) < 1e-9) return Rotate90(src, 2);
    if (std::fabs(d - 270.0) < 1e-9) return Rotate90(src, 3);

    double rad = d * kPi / 180.0;
    double c = std::cos(rad), s = std::sin(rad);
    double nwF = std::fabs(src.w * c) + std::fabs(src.h * s);
    double nhF = std::fabs(src.w * s) + std::fabs(src.h * c);
    int nw = (int)std::ceil(nwF - 1e-6) + 1;
    int nh = (int)std::ceil(nhF - 1e-6) + 1;
    FImage dst(nw, nh);
    double scx = src.w / 2.0, scy = src.h / 2.0;
    double dcx = nw / 2.0, dcy = nh / 2.0;
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            // Odwrotne odwzorowanie: obrot o -d wokol srodka.
            double dx = x + 0.5 - dcx, dy = y + 0.5 - dcy;
            double sx = c * dx + s * dy + scx - 0.5;
            double sy = -s * dx + c * dy + scy - 0.5;
            int ix = (int)std::floor(sx), iy = (int)std::floor(sy);
            double fx = sx - ix, fy = sy - iy;
            double wx[4], wy[4];
            for (int k = 0; k < 4; ++k) { wx[k] = CubicWeight(fx - (k - 1)); wy[k] = CubicWeight(fy - (k - 1)); }
            double acc[4] = {0, 0, 0, 0};
            for (int j = 0; j < 4; ++j) {
                int yy = iy + j - 1;
                if (yy < 0 || yy >= src.h) continue;
                for (int i = 0; i < 4; ++i) {
                    int xx = ix + i - 1;
                    if (xx < 0 || xx >= src.w) continue;
                    double w = wx[i] * wy[j];
                    const float* p = src.px(xx, yy);
                    for (int ch = 0; ch < 4; ++ch) acc[ch] += p[ch] * w;
                }
            }
            float* o = dst.px(x, y);
            float a = (float)std::min(std::max(acc[3], 0.0), 1.0);
            o[3] = a;
            for (int ch = 0; ch < 3; ++ch) o[ch] = (float)std::min(std::max(acc[ch], 0.0), (double)a);
        }
    }
    return dst;
}

void GaussianBlur(Mask& m, double sigma) {
    if (sigma <= 0.05 || m.w == 0 || m.h == 0) return;
    int r = (int)std::ceil(sigma * 3.0);
    std::vector<float> k(2 * r + 1);
    double sum = 0;
    for (int i = -r; i <= r; ++i) { double v = std::exp(-(i * i) / (2.0 * sigma * sigma)); k[i + r] = (float)v; sum += v; }
    for (auto& v : k) v = (float)(v / sum);

    Mask tmp(m.w, m.h);
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x) {
            double acc = 0;
            int lo = std::max(-r, -x), hi = std::min(r, m.w - 1 - x);
            for (int i = lo; i <= hi; ++i) acc += m.at(x + i, y) * k[i + r];
            tmp.at(x, y) = (float)acc;
        }
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x) {
            double acc = 0;
            int lo = std::max(-r, -y), hi = std::min(r, m.h - 1 - y);
            for (int i = lo; i <= hi; ++i) acc += tmp.at(x, y + i) * k[i + r];
            m.at(x, y) = (float)acc;
        }
}

Mask AlphaOf(const FImage& img) {
    Mask m(img.w, img.h);
    for (size_t i = 0; i < m.d.size(); ++i) m.d[i] = img.d[i * 4 + 3];
    return m;
}

FImage Pad(const FImage& src, int pad) {
    if (pad <= 0) return src;
    FImage o(src.w + 2 * pad, src.h + 2 * pad);
    for (int y = 0; y < src.h; ++y)
        std::copy(src.px(0, y), src.px(0, y) + (size_t)src.w * 4, o.px(pad, y + pad));
    return o;
}

bool CropToContent(const FImage& src, float threshold, FImage& out, int& outX, int& outY) {
    int minx = src.w, miny = src.h, maxx = -1, maxy = -1;
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x)
            if (src.px(x, y)[3] > threshold) {
                minx = std::min(minx, x); maxx = std::max(maxx, x);
                miny = std::min(miny, y); maxy = std::max(maxy, y);
            }
    if (maxx < 0) return false;
    out = FImage(maxx - minx + 1, maxy - miny + 1);
    for (int y = miny; y <= maxy; ++y)
        std::copy(src.px(minx, y), src.px(minx, y) + (size_t)out.w * 4, out.px(0, y - miny));
    outX = minx;
    outY = miny;
    return true;
}

namespace {

// Kwadrat odleglosci euklidesowej do najblizszego "zrodla" (Felzenszwalb-Huttenlocher).
// f[i] = 0 dla zrodla, duza liczba w przeciwnym razie.
void Edt1D(const double* f, int n, double* d, int* v, double* z) {
    const double kBig = 1e30;
    int k = 0;
    v[0] = 0;
    z[0] = -kBig;
    z[1] = kBig;
    for (int q = 1; q < n; ++q) {
        double s;
        while (true) {
            int p = v[k];
            s = ((f[q] + (double)q * q) - (f[p] + (double)p * p)) / (2.0 * (q - p));
            if (s <= z[k]) --k; else break;
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = kBig;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < q) ++k;
        double dq = (double)(q - v[k]);
        d[q] = dq * dq + f[v[k]];
    }
}

// Odleglosc (px) kazdego piksela do najblizszego piksela ze zbioru `src` (wartosc != 0).
std::vector<float> DistanceTo(const std::vector<uint8_t>& src, int w, int h) {
    const double kInf = 1e12;
    std::vector<double> g((size_t)w * h);
    for (size_t i = 0; i < g.size(); ++i) g[i] = src[i] ? 0.0 : kInf;
    int n = std::max(w, h);
    std::vector<double> f(n), d(n), z(n + 1);
    std::vector<int> v(n);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) f[y] = g[(size_t)y * w + x];
        Edt1D(f.data(), h, d.data(), v.data(), z.data());
        for (int y = 0; y < h; ++y) g[(size_t)y * w + x] = d[y];
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) f[x] = g[(size_t)y * w + x];
        Edt1D(f.data(), w, d.data(), v.data(), z.data());
        for (int x = 0; x < w; ++x) g[(size_t)y * w + x] = d[x];
    }
    std::vector<float> out(g.size());
    for (size_t i = 0; i < g.size(); ++i) out[i] = g[i] >= kInf * 0.5 ? 1e6f : (float)std::sqrt(g[i]);
    return out;
}

}  // namespace

Mask SignedDistance(const Mask& a) {
    int w = a.w, h = a.h;
    std::vector<uint8_t> inside((size_t)w * h), outside((size_t)w * h);
    bool anyIn = false, anyOut = false;
    for (size_t i = 0; i < inside.size(); ++i) {
        inside[i] = a.d[i] >= 0.5f;
        outside[i] = !inside[i];
        anyIn |= inside[i] != 0;
        anyOut |= outside[i] != 0;
    }
    Mask sd(w, h);
    if (!anyIn) { std::fill(sd.d.begin(), sd.d.end(), 1e6f); return sd; }
    std::vector<float> dOut = DistanceTo(inside, w, h);                       // dla pikseli na zewnatrz
    std::vector<float> dIn = anyOut ? DistanceTo(outside, w, h) : std::vector<float>((size_t)w * h, 1e6f);
    for (size_t i = 0; i < sd.d.size(); ++i) {
        float av = a.d[i];
        if (av > 0.0f && av < 1.0f) sd.d[i] = 0.5f - av;
        else if (av >= 1.0f) sd.d[i] = -(dIn[i] - 0.5f);
        else sd.d[i] = dOut[i] - 0.5f;
    }
    return sd;
}

}  // namespace wm
