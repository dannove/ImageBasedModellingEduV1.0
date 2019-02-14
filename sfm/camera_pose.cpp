//
// Created by cao on 18-11-14.
//

#include "camera_pose.h"

SFM_NAMESPACE_BEGIN

    bool
    CameraPose::FindRTS(const Mat &x1, const Mat &x2, double *S, Vec3 *t, Mat3 *R)
    {
        if ( x1.cols() < 3 || x2.cols() < 3 )
        {
            return false;
        }

        assert( 3 == x1.rows() );
        assert( 3 <= x1.cols() );
        assert( x1.rows() == x2.rows() );
        assert( x1.cols() == x2.cols() );

        // Get the transformation via Umeyama's least squares algorithm. This returns
        // a matrix of the form:
        // [ s * R t]
        // [ 0 1]
        // from which we can extract the scale, rotation, and translation.
        const Eigen::Matrix4d transform = Eigen::umeyama( x1, x2, true );

        // Check critical cases
        *R = transform.topLeftCorner<3, 3>();
        if ( R->determinant() < 0 )
        {
            return false;
        }
        *S = pow( R->determinant(), 1.0 / 3.0 );
        // Check for degenerate case (if all points have the same value...)
        if ( *S < std::numeric_limits<double>::epsilon() )
        {
            return false;
        }

        // Extract transformation parameters
        *S = pow( R->determinant(), 1.0 / 3.0 );
        *R /= *S;
        *t = transform.topRightCorner<3, 1>();

        return true;
    }

SFM_NAMESPACE_END