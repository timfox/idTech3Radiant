/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

/// \file
/// \brief Curve data types and related operations.

#include "debugging/debugging.h"
#include "container/array.h"
#include <math/matrix.h>


template<int I, int Degree>
double BernsteinPolynomial( double t ){
	     if constexpr( I == 0 && Degree == 0 ) return 1;
	else if constexpr( I == 0 && Degree == 1 ) return 1 - t;
	else if constexpr( I == 1 && Degree == 1 ) return t;
	else if constexpr( I == 0 && Degree == 2 ) return ( 1 - t ) * ( 1 - t );
	else if constexpr( I == 1 && Degree == 2 ) return 2 * ( 1 - t ) * t;
	else if constexpr( I == 2 && Degree == 2 ) return t * t;
	else if constexpr( I == 0 && Degree == 3 ) return ( 1 - t ) * ( 1 - t ) * ( 1 - t );
	else if constexpr( I == 1 && Degree == 3 ) return 3 * ( 1 - t ) * ( 1 - t ) * t;
	else if constexpr( I == 2 && Degree == 3 ) return 3 * ( 1 - t ) * t * t;
	else if constexpr( I == 3 && Degree == 3 ) return t * t * t;
	else static_assert( false, "general case not implemented" ); // assert on something false
}


typedef Array<Vector3> ControlPoints;

inline Vector3 CubicBezier_evaluate( const Vector3* firstPoint, double t ){
	Vector3 result( 0, 0, 0 );
	double denominator = 0;

	{
		double weight = BernsteinPolynomial<0, 3>( t );
		result += vector3_scaled( *firstPoint++, weight );
		denominator += weight;
	}
	{
		double weight = BernsteinPolynomial<1, 3>( t );
		result += vector3_scaled( *firstPoint++, weight );
		denominator += weight;
	}
	{
		double weight = BernsteinPolynomial<2, 3>( t );
		result += vector3_scaled( *firstPoint++, weight );
		denominator += weight;
	}
	{
		double weight = BernsteinPolynomial<3, 3>( t );
		result += vector3_scaled( *firstPoint++, weight );
		denominator += weight;
	}

	return result / denominator;
}

inline Vector3 CubicBezier_evaluateMid( const Vector3* firstPoint ){
	return vector3_scaled( firstPoint[0], 0.125 )
	     + vector3_scaled( firstPoint[1], 0.375 )
	     + vector3_scaled( firstPoint[2], 0.375 )
	     + vector3_scaled( firstPoint[3], 0.125 );
}

/// First derivative (tangent) of cubic Bezier at t: B'(t) = 3(1-t)^2(P1-P0) + 6(1-t)t(P2-P1) + 3t^2(P3-P2)
inline Vector3 CubicBezier_derivative( const Vector3* firstPoint, double t ){
	const double u = 1.0 - t;
	const Vector3 d0 = firstPoint[1] - firstPoint[0];
	const Vector3 d1 = firstPoint[2] - firstPoint[1];
	const Vector3 d2 = firstPoint[3] - firstPoint[2];
	return vector3_scaled( d0, 3.0 * u * u )
	     + vector3_scaled( d1, 6.0 * u * t )
	     + vector3_scaled( d2, 3.0 * t * t );
}

/// Bezier chain: points [P0,P1,P2,P3,P4,P5,P6,...] with segments (P0,P1,P2,P3), (P3,P4,P5,P6), ...
/// t in [0,1] maps to full chain. Requires at least 4 points.
inline Vector3 BezierChain_evaluate( const ControlPoints& points, double t ){
	if ( points.size() < 4 ) return points.empty() ? Vector3( 0, 0, 0 ) : points[0];
	const std::size_t numSegments = ( points.size() - 1 ) / 3;
	t = std::max( 0.0, std::min( 1.0, t ) ) * numSegments;
	std::size_t seg = static_cast<std::size_t>( t );
	double localT = t - seg;
	if ( seg >= numSegments ) { seg = numSegments - 1; localT = 1.0; }
	const Vector3* p = &points[seg * 3];
	return CubicBezier_evaluate( p, localT );
}

inline Vector3 BezierChain_derivative( const ControlPoints& points, double t ){
	if ( points.size() < 4 ) return Vector3( 0, 0, 0 );
	const std::size_t numSegments = ( points.size() - 1 ) / 3;
	t = std::max( 0.0, std::min( 1.0, t ) ) * numSegments;
	std::size_t seg = static_cast<std::size_t>( t );
	double localT = t - seg;
	if ( seg >= numSegments ) { seg = numSegments - 1; localT = 1.0; }
	const Vector3* p = &points[seg * 3];
	return CubicBezier_derivative( p, localT );
}

inline Vector3 CatmullRom_evaluate( const ControlPoints& controlPoints, double t ){
	// scale t to be segment-relative
	t *= double( controlPoints.size() - 1 );

	// subtract segment index;
	std::size_t segment = 0;
	for ( std::size_t i = 0; i < controlPoints.size() - 1; ++i )
	{
		if ( t <= double( i + 1 ) ) {
			segment = i;
			break;
		}
	}
	t -= segment;

	const double reciprocal_alpha = 1.0 / 3.0;

	Vector3 bezierPoints[4];
	bezierPoints[0] = controlPoints[segment];
	bezierPoints[1] = ( segment > 0 )
	                  ? controlPoints[segment] + vector3_scaled( controlPoints[segment + 1] - controlPoints[segment - 1], reciprocal_alpha * 0.5 )
	                  : controlPoints[segment] + vector3_scaled( controlPoints[segment + 1] - controlPoints[segment], reciprocal_alpha );
	bezierPoints[2] = ( segment < controlPoints.size() - 2 )
	                  ? controlPoints[segment + 1] + vector3_scaled( controlPoints[segment] - controlPoints[segment + 2], reciprocal_alpha * 0.5 )
	                  : controlPoints[segment + 1] + vector3_scaled( controlPoints[segment] - controlPoints[segment + 1], reciprocal_alpha );
	bezierPoints[3] = controlPoints[segment + 1];
	return CubicBezier_evaluate( bezierPoints, t );
}

inline Vector3 CatmullRom_derivative( const ControlPoints& controlPoints, double t ){
	if ( controlPoints.size() < 2 ) return Vector3( 0, 0, 0 );
	const double scale = double( controlPoints.size() - 1 );
	t *= scale;
	std::size_t segment = 0;
	for ( std::size_t i = 0; i < controlPoints.size() - 1; ++i )
	{
		if ( t <= double( i + 1 ) ) { segment = i; break; }
	}
	t -= segment;
	const double reciprocal_alpha = 1.0 / 3.0;
	Vector3 bezierPoints[4];
	bezierPoints[0] = controlPoints[segment];
	bezierPoints[1] = ( segment > 0 )
	                  ? controlPoints[segment] + vector3_scaled( controlPoints[segment + 1] - controlPoints[segment - 1], reciprocal_alpha * 0.5 )
	                  : controlPoints[segment] + vector3_scaled( controlPoints[segment + 1] - controlPoints[segment], reciprocal_alpha );
	bezierPoints[2] = ( segment < controlPoints.size() - 2 )
	                  ? controlPoints[segment + 1] + vector3_scaled( controlPoints[segment] - controlPoints[segment + 2], reciprocal_alpha * 0.5 )
	                  : controlPoints[segment + 1] + vector3_scaled( controlPoints[segment] - controlPoints[segment + 1], reciprocal_alpha );
	bezierPoints[3] = controlPoints[segment + 1];
	return vector3_scaled( CubicBezier_derivative( bezierPoints, t ), scale );
}

typedef Array<float> Knots;

inline double BSpline_basis( const Knots& knots, std::size_t i, std::size_t degree, double t ){
	if ( degree == 0 ) {
		if ( knots[i] <= t
		  && t < knots[i + 1]
		  && knots[i] < knots[i + 1] ) {
			return 1;
		}
		return 0;
	}
	double leftDenom = knots[i + degree] - knots[i];
	double left = ( leftDenom == 0 ) ? 0 : ( ( t - knots[i] ) / leftDenom ) * BSpline_basis( knots, i, degree - 1, t );
	double rightDenom = knots[i + degree + 1] - knots[i + 1];
	double right = ( rightDenom == 0 ) ? 0 : ( ( knots[i + degree + 1] - t ) / rightDenom ) * BSpline_basis( knots, i + 1, degree - 1, t );
	return left + right;
}

inline Vector3 BSpline_evaluate( const ControlPoints& controlPoints, const Knots& knots, std::size_t degree, double t ){
	Vector3 result( 0, 0, 0 );
	for ( std::size_t i = 0; i < controlPoints.size(); ++i )
	{
		result += vector3_scaled( controlPoints[i], BSpline_basis( knots, i, degree, t ) );
	}
	return result;
}

typedef Array<float> NURBSWeights;

inline Vector3 NURBS_evaluate( const ControlPoints& controlPoints, const NURBSWeights& weights, const Knots& knots, std::size_t degree, double t ){
	Vector3 result( 0, 0, 0 );
	double denominator = 0;
	for ( std::size_t i = 0; i < controlPoints.size(); ++i )
	{
		double weight = weights[i] * BSpline_basis( knots, i, degree, t );
		result += vector3_scaled( controlPoints[i], weight );
		denominator += weight;
	}
	return result / denominator;
}

inline void KnotVector_openUniform( Knots& knots, std::size_t count, std::size_t degree ){
	knots.resize( count + degree + 1 );

	std::size_t equalKnots = 1;

	for ( std::size_t i = 0; i < equalKnots; ++i )
	{
		knots[i] = 0;
		knots[knots.size() - ( i + 1 )] = 1;
	}

	std::size_t difference = knots.size() - 2 * ( equalKnots );
	for ( std::size_t i = 0; i < difference; ++i )
	{
		knots[i + equalKnots] = Knots::value_type( double( i + 1 ) * 1.0 / double( difference + 1 ) );
	}
}
