// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
 *   File "ba_b_tapenade_generated.c" is generated by Tapenade 3.14 (r7259) from this file.
 *   To reproduce such a generation you can use Tapenade CLI
 *   (can be downloaded from http://www-sop.inria.fr/tropics/tapenade/downloading.html)
 *
 *   After installing use the next command to generate a file:
 *
 *      tapenade -b -o ba_tapenade -head "compute_reproj_error(err)/(cam X) compute_zach_weight_error(err)/(w)" ba.c
 *
 *   This will produce a file "ba_tapenade_b.c" which content will be the same as the content of "ba_b_tapenade_generated.c",
 *   except one-line header. Moreover a log-file "ba_tapenade_b.msg" will be produced.
 *
 *   NOTE: the code in "ba_b_tapenade_generated.c" is wrong and won't work.
 *         REPAIRED SOURCE IS STORED IN THE FILE "ba_b.c".
 *         You can either use diff tool or read "ba_b.c" header to figure out what changes was performed to fix the code.
 *
 *   NOTE: you can also use Tapenade web server (http://tapenade.inria.fr:8080/tapenade/index.jsp)
 *         for generating but the result can be slightly different.
 */

#include "../adbench/ba.h"

extern "C" {

#include "ba.h"

/* ===================================================================== */
/*                                UTILS                                  */
/* ===================================================================== */

double sqsum(int n, double const* x)
{
    int i;
    double res = 0;
    for (i = 0; i < n; i++)
    {
        res = res + x[i] * x[i];
    }

    return res;
}

void cross_restrict(double const *__restrict a, double const *__restrict b,
                    double *__restrict out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

/* ===================================================================== */
/*                               MAIN LOGIC                              */
/* ===================================================================== */

// rot: 3 rotation parameters
// pt: 3 point to be rotated
// rotatedPt: 3 rotated point
// this is an efficient evaluation (part of
// the Ceres implementation)
// easy to understand calculation in matlab:
//  theta = sqrt(sum(w. ^ 2));
//  n = w / theta;
//  n_x = au_cross_matrix(n);
//  R = eye(3) + n_x*sin(theta) + n_x*n_x*(1 - cos(theta));
void rodrigues_rotate_point_restrict(double const *__restrict rot,
                                     double const *__restrict pt,
                                     double *__restrict rotatedPt) {
    int i;
    double sqtheta = sqsum(3, rot);
    if (sqtheta != 0)
    {
        double theta, costheta, sintheta, theta_inverse;
        double w[3], w_cross_pt[3], tmp;

        theta = sqrt(sqtheta);
        costheta = cos(theta);
        sintheta = sin(theta);
        theta_inverse = 1.0 / theta;

        for (i = 0; i < 3; i++)
        {
            w[i] = rot[i] * theta_inverse;
        }

        cross_restrict(w, pt, w_cross_pt);

        tmp = (w[0] * pt[0] + w[1] * pt[1] + w[2] * pt[2]) *
            (1. - costheta);

        for (i = 0; i < 3; i++)
        {
            rotatedPt[i] = pt[i] * costheta + w_cross_pt[i] * sintheta + w[i] * tmp;
        }
    }
    else
    {
        double rot_cross_pt[3];
        cross_restrict(rot, pt, rot_cross_pt);

        for (i = 0; i < 3; i++)
        {
            rotatedPt[i] = pt[i] + rot_cross_pt[i];
        }
    }
}

void radial_distort(double const* rad_params, double *proj)
{
    double rsq, L;
    rsq = sqsum(2, proj);
    L = 1. + rad_params[0] * rsq + rad_params[1] * rsq * rsq;
    proj[0] = proj[0] * L;
    proj[1] = proj[1] * L;
}

void radial_distort_restrict(double const *__restrict rad_params, double *__restrict proj)
{
    double rsq, L;
    rsq = sqsum(2, proj);
    L = 1. + rad_params[0] * rsq + rad_params[1] * rsq * rsq;
    proj[0] = proj[0] * L;
    proj[1] = proj[1] * L;
}

void project_restrict(double const *__restrict cam, double const *__restrict X,
                      double *__restrict proj) {
    double const* C = &cam[3];
    double Xo[3], Xcam[3];

    Xo[0] = X[0] - C[0];
    Xo[1] = X[1] - C[1];
    Xo[2] = X[2] - C[2];

    rodrigues_rotate_point_restrict(&cam[0], Xo, Xcam);

    proj[0] = Xcam[0] / Xcam[2];
    proj[1] = Xcam[1] / Xcam[2];

    radial_distort_restrict(&cam[9], proj);

    proj[0] = proj[0] * cam[6] + cam[7];
    proj[1] = proj[1] * cam[6] + cam[8];
}

// cam: 11 camera in format [r1 r2 r3 C1 C2 C3 f u0 v0 k1 k2]
//            r1, r2, r3 are angle - axis rotation parameters(Rodrigues)
//            [C1 C2 C3]' is the camera center
//            f is the focal length in pixels
//            [u0 v0]' is the principal point
//            k1, k2 are radial distortion parameters
// X: 3 point
// feats: 2 feature (x,y coordinates)
// reproj_err: 2
// projection function:
// Xcam = R * (X - C)
// distorted = radial_distort(projective2euclidean(Xcam), radial_parameters)
// proj = distorted * f + principal_point
// err = sqsum(proj - measurement)
void compute_reproj_error_restrict(double const *__restrict cam,
                                   double const *__restrict X,
                                   double const *__restrict w,
                                   double const *__restrict feat,
                                   double *__restrict err) {
    double proj[2];
    project_restrict(cam, X, proj);

    err[0] = (*w)*(proj[0] - feat[0]);
    err[1] = (*w)*(proj[1] - feat[1]);
}

void compute_zach_weight_error_restrict(double const *__restrict w,
                                        double *__restrict err) {
    *err = 1 - (*w)*(*w);
}

// n number of cameras
// m number of points
// p number of observations
// cams: 11*n cameras in format [r1 r2 r3 C1 C2 C3 f u0 v0 k1 k2]
//            r1, r2, r3 are angle - axis rotation parameters(Rodrigues)
//            [C1 C2 C3]' is the camera center
//            f is the focal length in pixels
//            [u0 v0]' is the principal point
//            k1, k2 are radial distortion parameters
// X: 3*m points
// obs: 2*p observations (pairs cameraIdx, pointIdx)
// feats: 2*p features (x,y coordinates corresponding to observations)
// reproj_err: 2*p errors of observations
// w_err: p weight "error" terms
void ba_objective_restrict(int n, int m, int p, double const *cams,
                           double const *X, double const *w, int const *obs,
                           double const *feats, double *reproj_err,
                           double *w_err) {
    int i;
    for (i = 0; i < p; i++)
    {
        int camIdx = obs[i * 2 + 0];
        int ptIdx = obs[i * 2 + 1];
        compute_reproj_error_restrict(&cams[camIdx * BA_NCAMPARAMS],
                                      &X[ptIdx * 3], &w[i], &feats[i * 2],
                                      &reproj_err[2 * i]);
    }

    for (i = 0; i < p; i++)
    {
        compute_zach_weight_error_restrict(&w[i], &w_err[i]);
    }
}

extern int enzyme_const;
extern int enzyme_dup;
extern int enzyme_dupnoneed;
void __enzyme_autodiff(...) noexcept;

void dcompute_reproj_error_restrict(double const *cam, double *dcam,
                                    double const *X, double *dX,
                                    double const *w, double *wb,
                                    double const *feat, double *err,
                                    double *derr) {
    __enzyme_autodiff(compute_reproj_error_restrict, enzyme_dup, cam, dcam,
                      enzyme_dup, X, dX, enzyme_dup, w, wb, enzyme_const, feat,
                      enzyme_dupnoneed, err, derr);
}

void dcompute_zach_weight_error_restrict(double const *w, double *dw,
                                         double *err, double *derr) {
    __enzyme_autodiff(compute_zach_weight_error_restrict, enzyme_dup, w, dw,
                      enzyme_dupnoneed, err, derr);
}
}


//! Tapenade
extern "C" {

#include <adBuffer.h>

/*
  Differentiation of sqsum in reverse (adjoint) mode:
   gradient     of useful results: *x sqsum
   with respect to varying inputs: *x
   Plus diff mem management of: x:in

 =====================================================================
                                UTILS
 ===================================================================== */
void sqsum_b(int n, const double *x, double *xb, double sqsumb) {
    int i;
    double res = 0;
    double resb = 0.0;
    double sqsum;
    resb = sqsumb;
    for (i = n-1; i > -1; --i)
        xb[i] = xb[i] + 2*x[i]*resb;
}

/* =====================================================================
                                UTILS
 ===================================================================== */
double sqsum_nodiff(int n, const double *x) {
    int i;
    double res = 0;
    for (i = 0; i < n; ++i)
        res = res + x[i]*x[i];
    return res;
}

/*
  Differentiation of cross in reverse (adjoint) mode:
   gradient     of useful results: *out *a *b
   with respect to varying inputs: *a *b
   Plus diff mem management of: out:in a:in b:in
*/
void cross_b(const double *a, double *ab, const double *b, double *bb, double
        *out, double *outb) {
    ab[0] = ab[0] + b[1]*outb[2];
    bb[1] = bb[1] + a[0]*outb[2];
    ab[1] = ab[1] - b[0]*outb[2];
    bb[0] = bb[0] - a[1]*outb[2];
    outb[2] = 0.0;
    ab[2] = ab[2] + b[0]*outb[1];
    bb[0] = bb[0] + a[2]*outb[1];
    ab[0] = ab[0] - b[2]*outb[1];
    bb[2] = bb[2] - a[0]*outb[1];
    outb[1] = 0.0;
    ab[1] = ab[1] + b[2]*outb[0];
    bb[2] = bb[2] + a[1]*outb[0];
    ab[2] = ab[2] - b[1]*outb[0];
    bb[1] = bb[1] - a[2]*outb[0];
}

void cross_nodiff(const double *a, const double *b, double *out) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

/*
  Differentiation of rodrigues_rotate_point in reverse (adjoint) mode:
   gradient     of useful results: *rot *rotatedPt
   with respect to varying inputs: *rot *pt
   Plus diff mem management of: rot:in rotatedPt:in pt:in

 =====================================================================
                               MAIN LOGIC
 ===================================================================== */
// rot: 3 rotation parameters
// pt: 3 point to be rotated
// rotatedPt: 3 rotated point
// this is an efficient evaluation (part of
// the Ceres implementation)
// easy to understand calculation in matlab:
//  theta = sqrt(sum(w. ^ 2));
//  n = w / theta;
//  n_x = au_cross_matrix(n);
//  R = eye(3) + n_x*sin(theta) + n_x*n_x*(1 - cos(theta));
void rodrigues_rotate_point_b(const double *rot, double *rotb, const double *
        pt, double *ptb, double *rotatedPt, double *rotatedPtb) {
    int i;
    double sqtheta;
    double sqthetab;
    int ii1;
    sqtheta = sqsum_nodiff(3, rot);
    if (sqtheta != 0) {
        double theta, costheta, sintheta, theta_inverse;
        double w[3], w_cross_pt[3], tmp;
        double tempb;
        theta = sqrt(sqtheta);
        costheta = cos(theta);
        sintheta = sin(theta);
        theta_inverse = 1.0/theta;
        double thetab, costhetab, sinthetab, theta_inverseb;
        double wb[3], w_cross_ptb[3], tmpb;
        for (i = 0; i < 3; ++i)
            w[i] = rot[i]*theta_inverse;
        cross_nodiff(w, pt, w_cross_pt);
        tmp = (w[0]*pt[0]+w[1]*pt[1]+w[2]*pt[2])*(1.-costheta);
        for (i = 0; i < 3; i++) /* TFIX */
            ptb[i] = 0.0;
        for (ii1 = 0; ii1 < 3; ++ii1)
            w_cross_ptb[ii1] = 0.0;
        for (ii1 = 0; ii1 < 3; ++ii1)
            wb[ii1] = 0.0;
        costhetab = 0.0;
        tmpb = 0.0;
        sinthetab = 0.0;
        for (i = 2; i > -1; --i) {
            ptb[i] = ptb[i] + costheta*rotatedPtb[i];
            costhetab = costhetab + pt[i]*rotatedPtb[i];
            w_cross_ptb[i] = w_cross_ptb[i] + sintheta*rotatedPtb[i];
            sinthetab = sinthetab + w_cross_pt[i]*rotatedPtb[i];
            wb[i] = wb[i] + tmp*rotatedPtb[i];
            tmpb = tmpb + w[i]*rotatedPtb[i];
            rotatedPtb[i] = 0.0;
        }
        tempb = (1.-costheta)*tmpb;
        wb[0] = wb[0] + pt[0]*tempb;
        ptb[0] = ptb[0] + w[0]*tempb;
        wb[1] = wb[1] + pt[1]*tempb;
        ptb[1] = ptb[1] + w[1]*tempb;
        wb[2] = wb[2] + pt[2]*tempb;
        ptb[2] = ptb[2] + w[2]*tempb;
        costhetab = costhetab - (w[0]*pt[0]+w[1]*pt[1]+w[2]*pt[2])*tmpb;
        cross_b(w, wb, pt, ptb, w_cross_pt, w_cross_ptb);
        theta_inverseb = 0.0;
        for (i = 2; i > -1; --i) {
            rotb[i] = rotb[i] + theta_inverse*wb[i];
            theta_inverseb = theta_inverseb + rot[i]*wb[i];
            wb[i] = 0.0;
        }
        thetab = cos(theta)*sinthetab - sin(theta)*costhetab - theta_inverseb/
            (theta*theta);
        if (sqtheta == 0.0)
            sqthetab = 0.0;
        else
            sqthetab = thetab/(2.0*sqrt(sqtheta));
    } else {
        {
          double rot_cross_pt[3];
          double rot_cross_ptb[3];
          for (i = 0; i < 3; i++) /* TFIX */
              ptb[i] = 0.0;
          for (ii1 = 0; ii1 < 3; ++ii1)
              rot_cross_ptb[ii1] = 0.0;
          for (i = 2; i > -1; --i) {
              ptb[i] = ptb[i] + rotatedPtb[i];
              rot_cross_ptb[i] = rot_cross_ptb[i] + rotatedPtb[i];
              rotatedPtb[i] = 0.0;
          }
          cross_b(rot, rotb, pt, ptb, rot_cross_pt, rot_cross_ptb);
        }
        sqthetab = 0.0;
    }
    sqsum_b(3, rot, rotb, sqthetab);
}

/* =====================================================================
                               MAIN LOGIC
 ===================================================================== */
// rot: 3 rotation parameters
// pt: 3 point to be rotated
// rotatedPt: 3 rotated point
// this is an efficient evaluation (part of
// the Ceres implementation)
// easy to understand calculation in matlab:
//  theta = sqrt(sum(w. ^ 2));
//  n = w / theta;
//  n_x = au_cross_matrix(n);
//  R = eye(3) + n_x*sin(theta) + n_x*n_x*(1 - cos(theta));
void rodrigues_rotate_point_nodiff(const double *rot, const double *pt, double
        *rotatedPt) {
    int i;
    double sqtheta;
    sqtheta = sqsum_nodiff(3, rot);
    if (sqtheta != 0) {
        double theta, costheta, sintheta, theta_inverse;
        double w[3], w_cross_pt[3], tmp;
        theta = sqrt(sqtheta);
        costheta = cos(theta);
        sintheta = sin(theta);
        theta_inverse = 1.0/theta;
        for (i = 0; i < 3; ++i)
            w[i] = rot[i]*theta_inverse;
        cross_nodiff(w, pt, w_cross_pt);
        tmp = (w[0]*pt[0]+w[1]*pt[1]+w[2]*pt[2])*(1.-costheta);
        for (i = 0; i < 3; ++i)
            rotatedPt[i] = pt[i]*costheta + w_cross_pt[i]*sintheta + w[i]*tmp;
    } else {
        double rot_cross_pt[3];
        cross_nodiff(rot, pt, rot_cross_pt);
        for (i = 0; i < 3; ++i)
            rotatedPt[i] = pt[i] + rot_cross_pt[i];
    }
}

/*
  Differentiation of radial_distort in reverse (adjoint) mode:
   gradient     of useful results: *rad_params *proj
   with respect to varying inputs: *rad_params *proj
   Plus diff mem management of: rad_params:in proj:in
*/
void radial_distort_b(const double *rad_params, double *rad_paramsb, double *
        proj, double *projb) {
    double rsq, L;
    double rsqb, Lb;
    rsq = sqsum_nodiff(2, proj);
    L = 1. + rad_params[0]*rsq + rad_params[1]*rsq*rsq;
    pushReal8(proj[0]);
    proj[0] = proj[0]*L;
    Lb = proj[1]*projb[1];
    projb[1] = L*projb[1];
    popReal8(&(proj[0]));
    Lb = Lb + proj[0]*projb[0];
    projb[0] = L*projb[0];
    rad_paramsb[0] = rad_paramsb[0] + rsq*Lb;
    rsqb = (rad_params[1]*2*rsq+rad_params[0])*Lb;
    rad_paramsb[1] = rad_paramsb[1] + rsq*rsq*Lb;
    sqsum_b(2, proj, projb, rsqb);
}

void radial_distort_nodiff(const double *rad_params, double *proj) {
    double rsq, L;
    rsq = sqsum_nodiff(2, proj);
    L = 1. + rad_params[0]*rsq + rad_params[1]*rsq*rsq;
    proj[0] = proj[0]*L;
    proj[1] = proj[1]*L;
}

/*
  Differentiation of project in reverse (adjoint) mode:
   gradient     of useful results: *proj
   with respect to varying inputs: *cam *X
   Plus diff mem management of: cam:in X:in proj:in
*/
void project_b(const double *cam, double *camb, const double *X, double *Xb,
        double *proj, double *projb) {
    double *C = const_cast<double*>(&(cam[3]));
    double *Cb = const_cast<double*>(&(camb[3]));
    double Xo[3], Xcam[3];
    double Xob[3], Xcamb[3];
    int ii1;
    double tempb;
    double tempb0;
    for (ii1 = 0; ii1 < BA_NCAMPARAMS; ii1++) /* TFIX */
        camb[ii1] = 0.0;
    for (ii1 = 0; ii1 < 3; ii1++)
        Xb[ii1] = 0.0;
    Xo[0] = X[0] - C[0];
    Xo[1] = X[1] - C[1];
    Xo[2] = X[2] - C[2];
    rodrigues_rotate_point_nodiff(&(cam[0]), Xo, Xcam);
    proj[0] = Xcam[0]/Xcam[2];
    proj[1] = Xcam[1]/Xcam[2];
    pushReal8Array(proj, 2); /* TFIX */
    radial_distort_nodiff(&(cam[9]), proj);
    pushReal8(proj[0]);
    proj[0] = proj[0]*cam[6] + cam[7];
    camb[6] = camb[6] + proj[1]*projb[1];
    camb[8] = camb[8] + projb[1];
    projb[1] = cam[6]*projb[1];
    popReal8(&(proj[0]));
    camb[6] = camb[6] + proj[0]*projb[0];
    camb[7] = camb[7] + projb[0];
    projb[0] = cam[6]*projb[0];
    popReal8Array(proj, 2); /* TFIX */
    radial_distort_b(&(cam[9]), &(camb[9]), proj, projb);
    for (ii1 = 0; ii1 < 3; ++ii1)
        Xcamb[ii1] = 0.0;
    tempb = projb[1]/Xcam[2];
    Xcamb[1] = Xcamb[1] + tempb;
    Xcamb[2] = Xcamb[2] - Xcam[1]*tempb/Xcam[2];
    projb[1] = 0.0;
    tempb0 = projb[0]/Xcam[2];
    Xcamb[0] = Xcamb[0] + tempb0;
    Xcamb[2] = Xcamb[2] - Xcam[0]*tempb0/Xcam[2];
    rodrigues_rotate_point_b(&(cam[0]), &(camb[0]), Xo, Xob, Xcam, Xcamb);
    Xb[2] = Xb[2] + Xob[2];
    Cb[2] = Cb[2] - Xob[2];
    Xob[2] = 0.0;
    Xb[1] = Xb[1] + Xob[1];
    Cb[1] = Cb[1] - Xob[1];
    Xob[1] = 0.0;
    Xb[0] = Xb[0] + Xob[0];
    Cb[0] = Cb[0] - Xob[0];
}

void project_nodiff(const double *cam, const double *X, double *proj) {
    const double *C = &(cam[3]);
    double Xo[3], Xcam[3];
    Xo[0] = X[0] - C[0];
    Xo[1] = X[1] - C[1];
    Xo[2] = X[2] - C[2];
    rodrigues_rotate_point_nodiff(&(cam[0]), Xo, Xcam);
    proj[0] = Xcam[0]/Xcam[2];
    proj[1] = Xcam[1]/Xcam[2];
    radial_distort_nodiff(&(cam[9]), proj);
    proj[0] = proj[0]*cam[6] + cam[7];
    proj[1] = proj[1]*cam[6] + cam[8];
}

/*
  Differentiation of compute_reproj_error in reverse (adjoint) mode:
   gradient     of useful results: *err
   with respect to varying inputs: *err *w *cam *X
   RW status of diff variables: *err:in-out *w:out *cam:out *X:out
   Plus diff mem management of: err:in w:in cam:in X:in
*/
// cam: 11 camera in format [r1 r2 r3 C1 C2 C3 f u0 v0 k1 k2]
//            r1, r2, r3 are angle - axis rotation parameters(Rodrigues)
//            [C1 C2 C3]' is the camera center
//            f is the focal length in pixels
//            [u0 v0]' is the principal point
//            k1, k2 are radial distortion parameters
// X: 3 point
// feats: 2 feature (x,y coordinates)
// reproj_err: 2
// projection function:
// Xcam = R * (X - C)
// distorted = radial_distort(projective2euclidean(Xcam), radial_parameters)
// proj = distorted * f + principal_point
// err = sqsum(proj - measurement)
void compute_reproj_error_b(const double *cam, double *camb, const double *X,
        double *Xb, const double *w, double *wb, const double *feat, double *
        err, double *errb) {
    double proj[2];
    double projb[2];
    int ii1;
    pushReal8Array(proj, 2);
    project_nodiff(cam, X, proj);
    for (ii1 = 0; ii1 < 2; ++ii1)
        projb[ii1] = 0.0;
    *wb = (proj[1]-feat[1])*errb[1];
    projb[1] = projb[1] + (*w)*errb[1];
    errb[1] = 0.0;
    *wb = *wb + (proj[0]-feat[0])*errb[0];
    projb[0] = projb[0] + (*w)*errb[0];
    errb[0] = 0.0;
    popReal8Array(proj, 2);
    project_b(cam, camb, X, Xb, proj, projb);
}

/*
  Differentiation of compute_zach_weight_error in reverse (adjoint) mode:
   gradient     of useful results: *err
   with respect to varying inputs: *err *w
   RW status of diff variables: *err:in-out *w:out
   Plus diff mem management of: err:in w:in
*/
void compute_zach_weight_error_b(const double *w, double *wb, double *err,
        double *errb) {
    *wb = -(2*(*w)*(*errb));
    *errb = 0.0;
}

}


#include <adept_source.h>
#include <adept.h>
#include <adept_arrays.h>
using adept::adouble;
using adept::aVector;

namespace adeptTest {

template<typename T>
void cross(
    const T* const a,
    const T* const b,
    T* out)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

////////////////////////////////////////////////////////////
//////////////////// Declarations //////////////////////////
////////////////////////////////////////////////////////////

// cam: 11 camera in format [r1 r2 r3 C1 C2 C3 f u0 v0 k1 k2]
//            r1, r2, r3 are angle - axis rotation parameters(Rodrigues)
//            [C1 C2 C3]' is the camera center
//            f is the focal length in pixels
//            [u0 v0]' is the principal point
//            k1, k2 are radial distortion parameters
// X: 3 point
// feats: 2 feature (x,y coordinates)
// reproj_err: 2
// projection function:
// Xcam = R * (X - C)
// distorted = radial_distort(projective2euclidean(Xcam), radial_parameters)
// proj = distorted * f + principal_point
// err = sqsum(proj - measurement)
template<typename T>
void computeReprojError(
    const T* const cam,
    const T* const X,
    const T* const w,
    const double* const feat,
    T *err);

// w: 1
// w_err: 1
template<typename T>
void computeZachWeightError(const T* const w, T* err);

// n number of cameras
// m number of points
// p number of observations
// cams: 11*n cameras in format [r1 r2 r3 C1 C2 C3 f u0 v0 k1 k2]
//            r1, r2, r3 are angle - axis rotation parameters(Rodrigues)
//            [C1 C2 C3]' is the camera center
//            f is the focal length in pixels
//            [u0 v0]' is the principal point
//            k1, k2 are radial distortion parameters
// X: 3*m points
// obs: 2*p observations (pairs cameraIdx, pointIdx)
// feats: 2*p features (x,y coordinates corresponding to observations)
// reproj_err: 2*p errors of observations
// w_err: p weight "error" terms
// projection function:
// Xcam = R * (X - C)
// distorted = radial_distort(projective2euclidean(Xcam), radial_parameters)
// proj = distorted * f + principal_point
// err = sqsum(proj - measurement)
template<typename T>
void ba_objective(int n, int m, int p,
    const T* const cams,
    const T* const X,
    const T* const w,
    const int* const obs,
    const double* const feats,
    T* reproj_err,
    T* w_err);

// rot: 3 rotation parameters
// pt: 3 point to be rotated
// rotatedPt: 3 rotated point
// this is an efficient evaluation (part of
// the Ceres implementation)
// easy to understand calculation in matlab:
//  theta = sqrt(sum(w. ^ 2));
//  n = w / theta;
//  n_x = au_cross_matrix(n);
//  R = eye(3) + n_x*sin(theta) + n_x*n_x*(1 - cos(theta));
template<typename T>
void rodrigues_rotate_point(
    const T* const rot,
    const T* const pt,
    T *rotatedPt);

////////////////////////////////////////////////////////////
//////////////////// Definitions ///////////////////////////
////////////////////////////////////////////////////////////

template<typename T>
T sqsum(int n, const T* const x)
{
    T res = 0;
    for (int i = 0; i < n; i++)
        res = res + x[i] * x[i];
    return res;
}

template<typename T>
void rodrigues_rotate_point(
    const T* const rot,
    const T* const pt,
    T *rotatedPt)
{
    T sqtheta = sqsum(3, rot);
    if (sqtheta != 0)
    {
        T theta, costheta, sintheta, theta_inverse,
            w[3], w_cross_pt[3], tmp;

        theta = sqrt(sqtheta);
        costheta = cos(theta);
        sintheta = sin(theta);
        theta_inverse = 1.0 / theta;

        for (int i = 0; i < 3; i++)
            w[i] = rot[i] * theta_inverse;

        cross(w, pt, w_cross_pt);

        tmp = (w[0] * pt[0] + w[1] * pt[1] + w[2] * pt[2]) *
            (1. - costheta);

        for (int i = 0; i < 3; i++)
            rotatedPt[i] = pt[i] * costheta + w_cross_pt[i] * sintheta + w[i] * tmp;
    }
    else
    {
        T rot_cross_pt[3];
        cross(rot, pt, rot_cross_pt);

        for (int i = 0; i < 3; i++)
            rotatedPt[i] = pt[i] + rot_cross_pt[i];
    }
}

template<typename T>
void radial_distort(
    const T* const rad_params,
    T *proj)
{
    T rsq, L;
    rsq = sqsum(2, proj);
    L = 1. + rad_params[0] * rsq + rad_params[1] * rsq * rsq;
    proj[0] = proj[0] * L;
    proj[1] = proj[1] * L;
}

template<typename T>
void project(const T* const cam,
    const T* const X,
    T* proj)
{
    const T* const C = &cam[3];
    T Xo[3], Xcam[3];

    Xo[0] = X[0] - C[0];
    Xo[1] = X[1] - C[1];
    Xo[2] = X[2] - C[2];

    rodrigues_rotate_point(&cam[0], Xo, Xcam);

    proj[0] = Xcam[0] / Xcam[2];
    proj[1] = Xcam[1] / Xcam[2];

    radial_distort(&cam[9], proj);

    proj[0] = proj[0] * cam[6] + cam[7];
    proj[1] = proj[1] * cam[6] + cam[8];
}

template<typename T>
void computeReprojError(
    const T* const cam,
    const T* const X,
    const T* const w,
    const double* const feat,
    T *err)
{
    T proj[2];
    project(cam, X, proj);

    err[0] = (*w)*(proj[0] - feat[0]);
    err[1] = (*w)*(proj[1] - feat[1]);
}

template<typename T>
void computeZachWeightError(const T* const w, T* err)
{
    *err = 1 - (*w)*(*w);
}

template<typename T>
void ba_objective(int n, int m, int p,
    const T* const cams,
    const T* const X,
    const T* const w,
    const int* const obs,
    const double* const feats,
    T* reproj_err,
    T* w_err)
{
    for (int i = 0; i < p; i++)
    {
        int camIdx = obs[i * 2 + 0];
        int ptIdx = obs[i * 2 + 1];
        computeReprojError(&cams[camIdx * BA_NCAMPARAMS], &X[ptIdx * 3],
            &w[i], &feats[i * 2], &reproj_err[2 * i]);
    }

    for (int i = 0; i < p; i++)
    {
        computeZachWeightError(&w[i], &w_err[i]);
    }
}
};


void adept_compute_reproj_error(
    double const* cam,
    double * dcam,
    double const* X,
    double * dX,
    double const* w,
    double * wb,
    double const* feat,
    double *err,
    double *derr
)
{


    adept::Stack stack;

      adouble acam[BA_NCAMPARAMS];
      adept::set_values(acam, BA_NCAMPARAMS, cam);

      adouble aX[3];
      adept::set_values(aX, 3, X);

      adouble aw;
      aw.set_value(*w);

      adouble areproj_err[2];

      stack.new_recording();

      adeptTest::computeReprojError(acam, aX, &aw, feat, areproj_err);

      for(unsigned i=0; i<2; i++) areproj_err[i].set_gradient(derr[i]);

      stack.compute_adjoint();

      *wb = aw.get_gradient();
      adept::get_gradients(aX, 3, dX);
      adept::get_gradients(acam, BA_NCAMPARAMS, dcam);
}

void adept_compute_zach_weight_error(double const* w, double* dw, double* err, double* derr) {
    adept::Stack stack;

      adouble aw;
      aw.set_value(*w);

      adouble aw_err;

      stack.new_recording();
      adeptTest::computeZachWeightError(&aw, &aw_err);
      aw_err.set_gradient(1.);
      stack.compute_adjoint();

      *dw = aw.get_gradient();
}

#include "ba_mayalias.h"
