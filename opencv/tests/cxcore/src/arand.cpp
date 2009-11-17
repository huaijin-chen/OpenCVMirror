/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "cxcoretest.h"

using namespace cv;


class CV_RandTest : public CvTest
{
public:
    CV_RandTest();
protected:
    void run(int);
    bool check_pdf(const Mat& hist, double scale, double A, double B,
                   int dist_type, double& refval, double& realval);
};


CV_RandTest::CV_RandTest():
CvTest( "rand", "cvRandArr, cvRNG" )
{
    support_testing_modes = CvTS::CORRECTNESS_CHECK_MODE;
}

static double chi2_p95(int n)
{
    static float chi2_tab95[] = {
        3.841, 5.991, 7.815, 9.488, 11.07, 12.59, 14.07, 15.51, 16.92, 18.31, 19.68, 21.03,
        21.03, 22.36, 23.69, 25.00, 26.30, 27.59, 28.87, 30.14, 31.41, 32.67, 33.92, 35.17,
        36.42, 37.65, 38.89, 40.11, 41.34, 42.56, 43.77 };
    static const double xp = 1.64;
    CV_Assert(n >= 1);
    
    if( n <= 30 )
        return chi2_tab95[n-1];
    return n + sqrt(2*n)*xp + 0.6666666666666*(xp*xp - 1);
}

bool CV_RandTest::check_pdf(const Mat& hist, double scale, double A, double B,
                            int dist_type, double& refval, double& realval)
{
    Mat hist0(hist.size(), CV_32F);
    const int* H = (const int*)hist.data;
    float* H0 = ((float*)hist0.data);
    int i, hsz = hist.cols;
    
    double sum = 0;
    for( i = 0; i < hsz; i++ )
        sum += H[i];
    CV_Assert( fabs(1./sum - scale) < FLT_EPSILON );
    
    if( dist_type == CV_RAND_UNI )
    {
        float scale0 = (float)(1./hsz);
        for( i = 0; i < hsz; i++ )
            H0[i] = scale0;
    }
    else
    {
        double sum = 0, r = (hsz-1.)/2;
        double alpha = 2*sqrt(2.)/r, beta = -alpha*r;
        for( i = 0; i < hsz; i++ )
        {
            double x = i*alpha + beta;
            H0[i] = (float)exp(-x*x);
            sum += H0[i];
        }
        sum = 1./sum;
        for( i = 0; i < hsz; i++ )
            H0[i] = (float)(H0[i]*sum);
    }
    
    double chi2 = 0;
    for( i = 0; i < hsz; i++ )
    {
        double a = H0[i];
        double b = H[i]*scale;
        if( a > DBL_EPSILON )
            chi2 += (a - b)*(a - b)/(a + b);
    }
    
    double chi2_pval = chi2_p95(hsz - 1 - (dist_type == CV_RAND_NORMAL ? 2 : 0));
    return chi2 <= chi2_pval*0.01;
}

void CV_RandTest::run( int start_from )
{
    static int _ranges[][2] =
    {{ 0, 256 }, { -128, 128 }, { 0, 65536 }, { -32768, 32768 },
        { -1000000, 1000000 }, { -1000, 1000 }, { -1000, 1000 }};
    
    const int MAX_SDIM = 10;
    const int N = 1200000;
    const int maxSlice = 1000;
    const int MAX_HIST_SIZE = 1000;
    int progress = 0;
    
    CvRNG* rng = ts->get_rng();
    RNG tested_rng;
    test_case_count = 500;
    
    for( int idx = 0; idx < test_case_count; idx++ )
    {
        ts->update_context( this, idx, false );
        
        int depth = cvTsRandInt(rng) % (CV_64F+1);
        int c, cn = (cvTsRandInt(rng) % 4) + 1;
        int type = CV_MAKETYPE(depth, cn);
        int dist_type = cvTsRandInt(rng) % (CV_RAND_NORMAL+1);
        int i, k, SZ = N/cn;
        Scalar A, B;
        
        bool do_sphere_test = dist_type == CV_RAND_UNI;
        Mat arr[2], hist[4];
        int W[] = {0,0,0,0};
        
        arr[0].create(1, SZ, type);
        arr[1].create(1, SZ, type);
        
        for( c = 0; c < cn; c++ )
        {
            int a, b, hsz;
            if( dist_type == CV_RAND_UNI )
            {
                a = (int)(cvTsRandInt(rng) % (_ranges[depth][1] -
                        _ranges[depth][0])) + _ranges[depth][0];
                do
                {
                    b = (int)(cvTsRandInt(rng) % (_ranges[depth][1] -
                        _ranges[depth][0])) + _ranges[depth][0];
                }
                while( abs(a-b) <= 1 );
                if( a > b )
                    std::swap(a, b);
                
                hsz = min((unsigned)(b - a), (unsigned)MAX_HIST_SIZE);
                do_sphere_test = do_sphere_test && b - a >= 100;
            }
            else
            {
                int vrange = _ranges[depth][1] - _ranges[depth][0];
                int meanrange = vrange/16;
                int mindiv = MAX(vrange/20, 5);
                int maxdiv = MIN(vrange/8, 10000);
                
                a = cvTsRandInt(rng) % meanrange - meanrange/2 +
                              (_ranges[depth][0] + _ranges[depth][1])/2;
                b = cvTsRandInt(rng) % (maxdiv - mindiv) + mindiv;
                hsz = min((unsigned)b*9, (unsigned)MAX_HIST_SIZE);
            }
            A[c] = a;
            B[c] = b;
            hist[c].create(1, hsz, CV_32S); 
        }
        
        cv::RNG saved_rng = tested_rng;
        for( k = 0; k < 2; k++ )
        {
            tested_rng = saved_rng;
            int sz = 0, dsz = 0, slice;
            for( slice = 0; slice < maxSlice; slice++, sz += dsz )
            {
                dsz = slice+1 < maxSlice ? cvTsRandInt(rng) % (SZ - sz + 1) : SZ - sz;
                Mat aslice = arr[k].colRange(sz, sz + dsz);
                tested_rng.fill(aslice, dist_type, A, B);
            }
        }
        
        if( norm(arr[0], arr[1], NORM_INF) != 0 )
        {
            ts->printf( CvTS::LOG, "RNG output depends on the array lengths (some generated numbers get lost?)" );
            ts->set_failed_test_info( CvTS::FAIL_INVALID_OUTPUT );
            return;
        }
        
        for( c = 0; c < cn; c++ )
        {
            const uchar* data = arr[0].data;
            int* H = hist[c].ptr<int>();
            int HSZ = hist[c].cols;
            double minVal = dist_type == CV_RAND_UNI ? A[c] : A[c] - B[c]*4;
            double maxVal = dist_type == CV_RAND_UNI ? B[c] : A[c] + B[c]*4;
            double scale = HSZ/(maxVal - minVal);
            double delta = -minVal*scale;
            
            hist[c] = Scalar::all(0);
            
            for( i = c; i < SZ*cn; i += cn )
            {
                double val = depth == CV_8U ? ((const uchar*)data)[i] :
                    depth == CV_8S ? ((const schar*)data)[i] :
                    depth == CV_16U ? ((const ushort*)data)[i] :
                    depth == CV_16S ? ((const short*)data)[i] :
                    depth == CV_32S ? ((const int*)data)[i] :
                    depth == CV_32F ? ((const float*)data)[i] :
                                      ((const double*)data)[i];
                int ival = cvFloor(val*scale + delta);
                if( (unsigned)ival < (unsigned)HSZ )
                {
                    H[ival]++;
                    W[c]++;
                }
                else if( dist_type == CV_RAND_UNI )
                {
                    if( depth >= CV_32F && val == maxVal )
                    {
                        H[HSZ-1]++;
                        W[c]++;
                    }
                    else
                    {
                        putchar('^');
                    }
                }
            }
            
            if( dist_type == CV_RAND_UNI && W[c] != SZ )
            {
                ts->printf( CvTS::LOG, "Uniform RNG gave values out of the range [%g,%g) on channel %d/%d\n",
                           A[c], B[c], c, cn);
                ts->set_failed_test_info( CvTS::FAIL_INVALID_OUTPUT );
                return;
            }
            if( dist_type == CV_RAND_NORMAL && W[c] < SZ*.90)
            {
                ts->printf( CvTS::LOG, "Normal RNG gave too many values out of the range (%g+4*%g,%g+4*%g) on channel %d/%d\n",
                           A[c], B[c], A[c], B[c], c, cn);
                ts->set_failed_test_info( CvTS::FAIL_INVALID_OUTPUT );
                return;
            }
            double refval = 0, realval = 0;
            
            if( !check_pdf(hist[c], 1./W[c], A[c], B[c], dist_type, refval, realval) )
            {
                ts->printf( CvTS::LOG, "RNG failed Chi-square test "
                           "(got %g vs probable maximum %g) on channel %d/%d\n",
                           realval, refval, c, cn);
                ts->set_failed_test_info( CvTS::FAIL_INVALID_OUTPUT );
                return;
            }
        }
        
        // Monte-Carlo test. Compute volume of SDIM-dimensional sphere
        // inscribed in [-1,1]^SDIM cube.
        if( do_sphere_test )
        {
            int SDIM = cvTsRandInt(rng) % (MAX_SDIM-1) + 2;
            int N0 = (SZ*cn/SDIM), N = 0;
            double r2 = 0;
            const uchar* data = arr[0].data;
            double scale[4], delta[4];
            for( c = 0; c < cn; c++ )
            {
                scale[c] = 2./(B[c] - A[c]);
                delta[c] = -A[c]*scale[c] - 1;
            }
            
            for( i = k = c = 0; i <= SZ*cn - SDIM; i++, k++, c++ )
            {
                double val = depth == CV_8U ? ((const uchar*)data)[i] :
                    depth == CV_8S ? ((const schar*)data)[i] :
                    depth == CV_16U ? ((const ushort*)data)[i] :
                    depth == CV_16S ? ((const short*)data)[i] :
                    depth == CV_32S ? ((const int*)data)[i] :
                    depth == CV_32F ? ((const float*)data)[i] : ((const double*)data)[i];
                c &= c < cn ? -1 : 0;
                val = val*scale[c] + delta[c];
                r2 += val*val;
                if( k == SDIM-1 )
                {
                    N += r2 <= 1;
                    r2 = 0;
                    k = -1;
                }
            }
            
            double V = ((double)N/N0)*(1 << SDIM);
            
            // the theoretically computed volume
            int sdim = SDIM % 2;
            double V0 = sdim + 1;
            for( sdim += 2; sdim <= SDIM; sdim += 2 )
                V0 *= 2*CV_PI/sdim;
            
            if( fabs(V - V0) > 0.1*fabs(V0) )
            {
                ts->printf( CvTS::LOG, "RNG failed %d-dim sphere volume test (got %g instead of %g)\n",
                           SDIM, V, V0);
                ts->set_failed_test_info( CvTS::FAIL_INVALID_OUTPUT );
                return;
            }
        }
        progress = update_progress( progress, idx, test_case_count, 0 );
    }
}

CV_RandTest rand_test;
