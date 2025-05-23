#pragma once

#include "../../json.hpp"
#include "../mshared/defs.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
float tdiff(struct timeval *start, struct timeval *end) {
  return (end->tv_sec-start->tv_sec) + 1e-6*(end->tv_usec-start->tv_usec);
}

using namespace std;
using json = nlohmann::json;

struct GMMInput {
    size_t d, k, n;
    std::vector<double> alphas, means, icf, x;
    Wishart wishart;
};

struct GMMOutput {
    double objective;
    std::vector<double> gradient;
};

struct GMMParameters {
    bool replicate_point;
};

extern "C" {
void gmm_objective(size_t d, size_t k, size_t n, double const *alphas,
                   double const *means, double const *icf, double const *x,
                   Wishart wishart, double *err);
void gmm_objective_restrict(size_t d, size_t k, size_t n, double const *alphas,
                            double const *means, double const *icf,
                            double const *x, Wishart wishart, double *err);
void dgmm_objective_restrict(size_t d, size_t k, size_t n, const double *alphas,
                             double *alphasb, const double *means,
                             double *meansb, const double *icf, double *icfb,
                             const double *x, Wishart wishart, double *err,
                             double *errb);
void dgmm_objective(size_t d, size_t k, size_t n, const double *alphas, double *alphasb,
                    const double *means, double *meansb, const double *icf,
                    double *icfb, const double *x, Wishart wishart, double *err,
                    double *errb);

void gmm_objective_b(size_t d, size_t k, size_t n, const double *alphas, double *alphasb,
                     const double *means, double *meansb, const double *icf,
                     double *icfb, const double *x, Wishart wishart,
                     double *err, double *errb);

void adept_dgmm_objective(size_t d, size_t k, size_t n, const double *alphas,
                          double *alphasb, const double *means, double *meansb,
                          const double *icf, double *icfb, const double *x,
                          Wishart wishart, double *err, double *errb);

void rust_unsafe_dgmm_objective(size_t d, size_t k, size_t n, const double *alphas,
                                double *alphasb, const double *means,
                                double *meansb, const double *icf, double *icfb,
                                const double *x, Wishart &wishart, double *err,
                                double *errb);

void rust_unsafe_gmm_objective(size_t d, size_t k, size_t n, const double *alphas,
                               const double *means, const double *icf,
                               const double *x, Wishart &wishart, double *err);

void rust_dgmm_objective(size_t d, size_t k, size_t n, const double *alphas,
                         double *alphasb, const double *means, double *meansb,
                         const double *icf, double *icfb, const double *x,
                         Wishart &wishart, double *err, double *errb);

void rust_gmm_objective(size_t d, size_t k, size_t n, const double *alphas,
                        const double *means, const double *icf, const double *x,
                        Wishart &wishart, double *err);
}

void read_gmm_instance(const string& fn,
    size_t* d, size_t* k, size_t* n,
    vector<double>& alphas,
    vector<double>& means,
    vector<double>& icf,
    vector<double>& x,
    Wishart& wishart,
    bool replicate_point)
{
    FILE* fid = fopen(fn.c_str(), "r");

    if (!fid) {
        printf("could not open file: %s\n", fn.c_str());
        exit(1);
    }

    fscanf(fid, "%zu %zu %zu", d, k, n);

    size_t d_ = *d, k_ = *k, n_ = *n;

    size_t icf_sz = d_ * (d_ + 1) / 2;
    alphas.resize(k_);
    means.resize(d_ * k_);
    icf.resize(icf_sz * k_);
    x.resize(d_ * n_);

    for (size_t i = 0; i < k_; i++)
    {
        fscanf(fid, "%lf", &alphas[i]);
    }

    for (size_t i = 0; i < k_; i++)
    {
        for (size_t j = 0; j < d_; j++)
        {
            fscanf(fid, "%lf", &means[i * d_ + j]);
        }
    }

    for (size_t i = 0; i < k_; i++)
    {
        for (size_t j = 0; j < icf_sz; j++)
        {
            fscanf(fid, "%lf", &icf[i * icf_sz + j]);
        }
    }

    if (replicate_point)
    {
        for (size_t j = 0; j < d_; j++)
        {
            fscanf(fid, "%lf", &x[j]);
        }
        for (size_t i = 0; i < n_; i++)
        {
            memcpy(&x[i * d_], &x[0], d_ * sizeof(double));
        }
    }
    else
    {
        for (size_t i = 0; i < n_; i++)
        {
            for (size_t j = 0; j < d_; j++)
            {
                fscanf(fid, "%lf", &x[i * d_ + j]);
            }
        }
    }

    fscanf(fid, "%lf %i", &(wishart.gamma), &(wishart.m));

    fclose(fid);
}

template<auto  deriv>
void calculate_jacobian(struct GMMInput &input, struct GMMOutput &result)
{
    double* alphas_gradient_part = result.gradient.data();
    double* means_gradient_part = result.gradient.data() + input.alphas.size();
    double* icf_gradient_part =
        result.gradient.data() +
        input.alphas.size() +
        input.means.size();

    double tmp = 0.0;       // stores fictive result
                            // (Tapenade doesn't calculate an original function in reverse mode)

    double errb = 1.0;      // stores dY
                            // (equals to 1.0 for gradient calculation)

    deriv(
        input.d,
        input.k,
        input.n,
        input.alphas.data(),
        alphas_gradient_part,
        input.means.data(),
        means_gradient_part,
        input.icf.data(),
        icf_gradient_part,
        input.x.data(),
        input.wishart,
        &tmp,
        &errb
    );
}

template<auto  deriv>
double primal(struct GMMInput &input)
{
    double tmp = 0.0;       // stores fictive result
                            // (Tapenade doesn't calculate an original function in reverse mode)
    deriv(
        input.d,
        input.k,
        input.n,
        input.alphas.data(),
        input.means.data(),
        input.icf.data(),
        input.x.data(),
        input.wishart,
        &tmp
    );
    return tmp;
}

int main(const int argc, const char* argv[]) {
    printf("starting main\n");

    const auto replicate_point = (argc > 9 && string(argv[9]) == "-rep");
    const GMMParameters params = { replicate_point };

    std::vector<std::string> paths = { "10k/gmm_d10_K200.txt" };

    getTests(paths, "data/1k", "1k/");
    //if (std::getenv("BENCH_LARGE")) {
      getTests(paths, "data/2.5k", "2.5k/");
      getTests(paths, "data/10k", "10k/");
    //}

    std::ofstream jsonfile("results.json", std::ofstream::trunc);
    json test_results;

    for (auto path : paths) {
    	if (path == "10k/gmm_d128_K200.txt" || path == "10k/gmm_d128_K100.txt" || path == "10k/gmm_d64_K200.txt" || path == "10k/gmm_d128_K50.txt" || path == "10k/gmm_d64_K100.txt") continue;
        printf("starting path %s\n", path.c_str());
      json test_suite;
      test_suite["name"] = path;

    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
        input.alphas, input.means, input.icf, input.x, input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = { 0, std::vector<double>(Jcols) };

    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      calculate_jacobian<gmm_objective_b>(input, result);
      gettimeofday(&end, NULL);
      printf("Tapenade combined %0.6f\n", tdiff(&start, &end));
      json tapenade;
      tapenade["name"] = "Tapenade combined";
      tapenade["runtime"] = tdiff(&start, &end);
      for (unsigned i = result.gradient.size() - 5;
           i < result.gradient.size(); i++) {
        printf("%f ", result.gradient[i]);
        tapenade["result"].push_back(result.gradient[i]);
      }
      test_suite["tools"].push_back(tapenade);
      printf("\n");
    }

    }

    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
        input.alphas, input.means, input.icf, input.x, input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = { 0, std::vector<double>(Jcols) };

    if (1) {
      try {
        struct timeval start, end;
        gettimeofday(&start, NULL);
        calculate_jacobian<adept_dgmm_objective>(input, result);
        gettimeofday(&end, NULL);
        printf("Adept combined %0.6f\n", tdiff(&start, &end));
        json adept;
        adept["name"] = "Adept combined";
        adept["runtime"] = tdiff(&start, &end);
        for (unsigned i = result.gradient.size() - 5;
             i < result.gradient.size(); i++) {
          printf("%f ", result.gradient[i]);
          adept["result"].push_back(result.gradient[i]);
        }
        printf("\n");
        test_suite["tools"].push_back(adept);
      } catch (std::bad_alloc) {
        printf("Adept combined 88888888 ooms\n");
      }
    }
    }

    for (size_t i = 0; i < 5; i++)
    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
        input.alphas, input.means, input.icf, input.x, input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = { 0, std::vector<double>(Jcols) };

    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      calculate_jacobian<dgmm_objective_restrict>(input, result);
      gettimeofday(&end, NULL);
      printf("Enzyme c++ restrict combined %0.6f\n", tdiff(&start, &end));
      json enzyme;
      enzyme["name"] = "Enzyme restrict combined";
      enzyme["runtime"] = tdiff(&start, &end);
      for (unsigned i = result.gradient.size() - 5; i < result.gradient.size();
           i++) {
        printf("%f ", result.gradient[i]);
        enzyme["result"].push_back(result.gradient[i]);
      }
      printf("\n");
      test_suite["tools"].push_back(enzyme);
    }
    }

    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
                      input.alphas, input.means, input.icf, input.x,
                      input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = {0, std::vector<double>(Jcols)};

    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      calculate_jacobian<dgmm_objective>(input, result);
      gettimeofday(&end, NULL);
      printf("Enzyme c++ mayalias combined %0.6f\n", tdiff(&start, &end));
      json enzyme;
      enzyme["name"] = "Enzyme mayalias combined";
      enzyme["runtime"] = tdiff(&start, &end);
      for (unsigned i = result.gradient.size() - 5;
           i < result.gradient.size(); i++) {
        printf("%f ", result.gradient[i]);
        enzyme["result"].push_back(result.gradient[i]);
      }
      printf("\n");
      test_suite["tools"].push_back(enzyme);
    }
    }

    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
                      input.alphas, input.means, input.icf, input.x,
                      input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = {0, std::vector<double>(Jcols)};
    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      calculate_jacobian<rust_unsafe_dgmm_objective>(input, result);
      gettimeofday(&end, NULL);
      printf("Enzyme unsafe rust combined %0.6f\n", tdiff(&start, &end));
      json enzyme;
      enzyme["name"] = "Rust unsafe Enzyme combined";
      enzyme["runtime"] = tdiff(&start, &end);
      for (unsigned i = result.gradient.size() - 5; i < result.gradient.size();
           i++) {
        printf("%f ", result.gradient[i]);
        enzyme["result"].push_back(result.gradient[i]);
      }
      printf("\n");
      test_suite["tools"].push_back(enzyme);
    }
    }

    for (size_t i = 0; i < 5; i++)
    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
                      input.alphas, input.means, input.icf, input.x,
                      input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = {0, std::vector<double>(Jcols)};
    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      calculate_jacobian<rust_dgmm_objective>(input, result);
      gettimeofday(&end, NULL);
      printf("Enzyme rust combined %0.6f\n", tdiff(&start, &end));
      json enzyme;
      enzyme["name"] = "Rust Enzyme combined";
      enzyme["runtime"] = tdiff(&start, &end);
      for (unsigned i = result.gradient.size() - 5;
           i < result.gradient.size(); i++) {
        printf("%f ", result.gradient[i]);
        enzyme["result"].push_back(result.gradient[i]);
      }
      printf("\n");
      test_suite["tools"].push_back(enzyme);
    }
    }

    {

    struct GMMInput input;
    read_gmm_instance("data/" + path, &input.d, &input.k, &input.n,
        input.alphas, input.means, input.icf, input.x, input.wishart, params.replicate_point);

    size_t Jcols = (input.k * (input.d + 1) * (input.d + 2)) / 2;

    struct GMMOutput result = { 0, std::vector<double>(Jcols) };

    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      auto res = primal<gmm_objective>(input);
      gettimeofday(&end, NULL);
      printf("c++ primal mayalias combined t=%0.6f, err=%f\n",
             tdiff(&start, &end), res);

      json primal;
      primal["name"] = "C++ primal mayalias";
      primal["runtime"] = tdiff(&start, &end);
      primal["result"].push_back(res);
      test_suite["tools"].push_back(primal);
    }
    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      auto res = primal<gmm_objective_restrict>(input);
      gettimeofday(&end, NULL);
      printf("c++ primal restrict combined t=%0.6f, err=%f\n",
             tdiff(&start, &end), res);

      json primal;
      primal["name"] = "C++ primal restrict";
      primal["runtime"] = tdiff(&start, &end);
      primal["result"].push_back(res);
      test_suite["tools"].push_back(primal);
    }
    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      auto res = primal<rust_unsafe_gmm_objective>(input);
      gettimeofday(&end, NULL);
      printf("rust unsafe primal combined t=%0.6f, err=%f\n",
             tdiff(&start, &end), res);
      json primal;
      primal["name"] = "Rust unsafe primal";
      primal["runtime"] = tdiff(&start, &end);
      primal["result"].push_back(res);
      test_suite["tools"].push_back(primal);
    }
    {
      struct timeval start, end;
      gettimeofday(&start, NULL);
      auto res = primal<rust_gmm_objective>(input);
      gettimeofday(&end, NULL);
      printf("rust primal combined t=%0.6f, err=%f\n", tdiff(&start, &end), res);
      json primal;
      primal["name"] = "Rust primal";
      primal["runtime"] = tdiff(&start, &end);
      primal["result"].push_back(res);
      test_suite["tools"].push_back(primal);
    }
    }

    test_suite["llvm-version"] = __clang_version__;
    test_suite["mode"] = "ReverseMode";
    test_suite["batch-size"] = 1;
    test_results.push_back(test_suite);
   }
   jsonfile << std::setw(4) << test_results;
}
