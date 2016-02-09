/*!
 * \file volk_gnsssdr_16ic_x2_dot_prod_16ic_xn.h
 * \brief Volk protokernel: multiplies N 16 bits vectors by a common vector phase rotated and accumulates the results in N 16 bits short complex outputs.
 * \authors <ul>
 *          <li> Javier Arribas, 2015. jarribas(at)cttc.es
 *          </ul>
 *
 * Volk protokernel that multiplies N 16 bits vectors by a common vector, which is phase-rotated by phase offset and phase increment, and accumulates the results in N 16 bits short complex outputs.
 * It is optimized to perform the N tap correlation process in GNSS receivers.
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2015  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */

#ifndef INCLUDED_volk_gnsssdr_16ic_xn_rotator_dot_prod_16ic_xn_H
#define INCLUDED_volk_gnsssdr_16ic_xn_rotator_dot_prod_16ic_xn_H


#include <volk_gnsssdr/volk_gnsssdr_complex.h>
#include <volk_gnsssdr/saturation_arithmetic.h>

#ifdef LV_HAVE_GENERIC
/*!
 \brief Multiplies the reference complex vector with multiple versions of another complex vector, accumulates the results and stores them in the output vector
 \param[out] result        Array of num_a_vectors components with the multiple versions of in_a multiplied and accumulated The vector where the accumulated result will be stored
 \param[in]  in_common     Pointer to one of the vectors to be multiplied and accumulated (reference vector)
 \param[in]  in_a          Pointer to an array of pointers to multiple versions of the other vector to be multiplied and accumulated
 \param[in]  num_a_vectors Number of vectors to be multiplied by the reference vector and accumulated
 \param[in]  num_points    The Number of complex values to be multiplied together, accumulated and stored into result
 */
static inline void volk_gnsssdr_16ic_x2_rotator_dot_prod_16ic_xn_generic(lv_16sc_t* result, const lv_16sc_t* in_common, const lv_32fc_t phase_inc, lv_32fc_t* phase, const lv_16sc_t** in_a, int num_a_vectors, unsigned int num_points)
{
	lv_16sc_t tmp16;
	lv_32fc_t tmp32;
	for (int n_vec = 0; n_vec < num_a_vectors; n_vec++)
	{
		result[n_vec] = lv_cmake(0,0);
	}
	for (unsigned int n = 0; n < num_points; n++)
	{
		tmp16 = *in_common++;
		tmp32 = lv_cmake((float)lv_creal(tmp16), (float)lv_cimag(tmp16)) * (*phase);
		tmp16 = lv_cmake((int16_t)rintf(lv_creal(tmp32)), (int16_t)rintf(lv_cimag(tmp32)));
		(*phase) *= phase_inc;
		for (int n_vec = 0; n_vec < num_a_vectors; n_vec++)
		{
			lv_16sc_t tmp = tmp16 * in_a[n_vec][n];
			result[n_vec] = lv_cmake(sat_adds16i(lv_creal(result[n_vec]), lv_creal(tmp)), sat_adds16i(lv_cimag(result[n_vec]), lv_cimag(tmp)));
		}
	}
}

#endif /*LV_HAVE_GENERIC*/


#ifdef LV_HAVE_SSE3
#include <pmmintrin.h>

/*!
 \brief Multiplies the reference complex vector with multiple versions of another complex vector, accumulates the results and stores them in the output vector
 \param[out] result        Array of num_a_vectors components with the multiple versions of in_a multiplied and accumulated The vector where the accumulated result will be stored
 \param[in]  in_common     Pointer to one of the vectors to be multiplied and accumulated (reference vector)
 \param[in]  in_a          Pointer to an array of pointers to multiple versions of the other vector to be multiplied and accumulated
 \param[in]  num_a_vectors Number of vectors to be multiplied by the reference vector and accumulated
 \param[in]  num_points    The Number of complex values to be multiplied together, accumulated and stored into result
 */
static inline void volk_gnsssdr_16ic_x2_rotator_dot_prod_16ic_xn_a_sse3(lv_16sc_t* out, const lv_16sc_t* in_common, const lv_32fc_t phase_inc, lv_32fc_t* phase, const lv_16sc_t** in_a,  int num_a_vectors, unsigned int num_points)
{
	lv_16sc_t dotProduct = lv_cmake(0,0);

	const unsigned int sse_iters = num_points / 4;

	const lv_16sc_t** _in_a = in_a;
	const lv_16sc_t* _in_common = in_common;
	lv_16sc_t* _out = out;

	__VOLK_ATTR_ALIGNED(16) lv_16sc_t dotProductVector[4];

	//todo dyn mem reg

	__m128i* realcacc;
	__m128i* imagcacc;

	realcacc = (__m128i*)calloc(num_a_vectors, sizeof(__m128i)); //calloc also sets memory to 0
	imagcacc = (__m128i*)calloc(num_a_vectors, sizeof(__m128i)); //calloc also sets memory to 0

	__m128i a,b,c, c_sr, mask_imag, mask_real, real, imag, imag1,imag2, b_sl, a_sl, result;

	mask_imag = _mm_set_epi8(255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0);
	mask_real = _mm_set_epi8(0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255);

	// phase rotation registers
	__m128 pa, pb, two_phase_acc_reg, two_phase_inc_reg;
	__m128i pc1, pc2;
	__attribute__((aligned(16))) lv_32fc_t two_phase_inc[2];
	two_phase_inc[0] = phase_inc * phase_inc;
	two_phase_inc[1] = phase_inc * phase_inc;
	two_phase_inc_reg = _mm_load_ps((float*) two_phase_inc);
	__attribute__((aligned(16))) lv_32fc_t two_phase_acc[2];
	two_phase_acc[0] = (*phase);
	two_phase_acc[1] = (*phase) * phase_inc;
	two_phase_acc_reg = _mm_load_ps((float*)two_phase_acc);
	__m128 yl, yh, tmp1, tmp2, tmp3;
	lv_16sc_t tmp16;
	lv_32fc_t tmp32;

	for(unsigned int number = 0; number < sse_iters; number++)
	{
		// Phase rotation on operand in_common starts here:

		pa = _mm_set_ps((float)(lv_cimag(_in_common[1])), (float)(lv_creal(_in_common[1])), (float)(lv_cimag(_in_common[0])), (float)(lv_creal(_in_common[0]))); // //load (2 byte imag, 2 byte real) x 2 into 128 bits reg
		//complex 32fc multiplication b=a*two_phase_acc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(pa, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		pa = _mm_shuffle_ps(pa, pa, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(pa, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		pb = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di
		pc1 = _mm_cvtps_epi32(pb); // convert from 32fc to 32ic

		//complex 32fc multiplication two_phase_acc_reg=two_phase_acc_reg*two_phase_inc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(two_phase_inc_reg, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		tmp3 = _mm_shuffle_ps(two_phase_inc_reg, two_phase_inc_reg, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(tmp3, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		two_phase_acc_reg = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di

		//next two samples
		_in_common += 2;
		pa = _mm_set_ps((float)(lv_cimag(_in_common[1])), (float)(lv_creal(_in_common[1])), (float)(lv_cimag(_in_common[0])), (float)(lv_creal(_in_common[0]))); // //load (2 byte imag, 2 byte real) x 2 into 128 bits reg
		//complex 32fc multiplication b=a*two_phase_acc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(pa, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		pa = _mm_shuffle_ps(pa, pa, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(pa, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		pb = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di
		pc2 = _mm_cvtps_epi32(pb); // convert from 32fc to 32ic

		//complex 32fc multiplication two_phase_acc_reg=two_phase_acc_reg*two_phase_inc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(two_phase_inc_reg, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		tmp3 = _mm_shuffle_ps(two_phase_inc_reg, two_phase_inc_reg, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(tmp3, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		two_phase_acc_reg = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di

		// store four rotated in_common samples in the register b
		b = _mm_packs_epi32(pc1, pc2);// convert from 32ic to 16ic

		//next two samples
		_in_common += 2;

		for (int n_vec = 0; n_vec < num_a_vectors; n_vec++)
		{
			a = _mm_load_si128((__m128i*)&(_in_a[n_vec][number*4])); //load (2 byte imag, 2 byte real) x 4 into 128 bits reg

			c = _mm_mullo_epi16 (a, b); // a3.i*b3.i, a3.r*b3.r, ....

			c_sr = _mm_srli_si128 (c, 2); // Shift a right by imm8 bytes while shifting in zeros, and store the results in dst.
			real = _mm_subs_epi16 (c, c_sr);

			b_sl = _mm_slli_si128(b, 2); // b3.r, b2.i ....
			a_sl = _mm_slli_si128(a, 2); // a3.r, a2.i ....

			imag1 = _mm_mullo_epi16(a, b_sl); // a3.i*b3.r, ....
			imag2 = _mm_mullo_epi16(b, a_sl); // b3.i*a3.r, ....

			imag = _mm_adds_epi16(imag1, imag2);

			realcacc[n_vec] = _mm_adds_epi16 (realcacc[n_vec], real);
			imagcacc[n_vec] = _mm_adds_epi16 (imagcacc[n_vec], imag);

		}
	}

	for (int n_vec=0;n_vec<num_a_vectors;n_vec++)
	{
		realcacc[n_vec] = _mm_and_si128 (realcacc[n_vec], mask_real);
		imagcacc[n_vec] = _mm_and_si128 (imagcacc[n_vec], mask_imag);

		result = _mm_or_si128 (realcacc[n_vec], imagcacc[n_vec]);

		_mm_store_si128((__m128i*)dotProductVector, result); // Store the results back into the dot product vector
		dotProduct = lv_cmake(0,0);
		for (int i = 0; i<4; ++i)
		{
			dotProduct = lv_cmake(sat_adds16i(lv_creal(dotProduct), lv_creal(dotProductVector[i])),
					sat_adds16i(lv_cimag(dotProduct), lv_cimag(dotProductVector[i])));
		}
		_out[n_vec] = dotProduct;
	}
	free(realcacc);
	free(imagcacc);

	_mm_store_ps((float*)two_phase_acc, two_phase_acc_reg);
	(*phase) = lv_cmake(two_phase_acc[0], two_phase_acc[0]);

	for (int n_vec = 0; n_vec < num_a_vectors; n_vec++)
	{
		for(unsigned int n  = sse_iters * 4; n < num_points; n++)
		{
			tmp16 = *in_common++;
			tmp32 = lv_cmake((float)lv_creal(tmp16), (float)lv_cimag(tmp16)) * (*phase);
			tmp16 = lv_cmake((int16_t)rintf(lv_creal(tmp32)), (int16_t)rintf(lv_cimag(tmp32)));
			(*phase) *= phase_inc;
			lv_16sc_t tmp = tmp16 * in_a[n_vec][n];
			_out[n_vec] = lv_cmake(sat_adds16i(lv_creal(_out[n_vec]), lv_creal(tmp)),
					sat_adds16i(lv_cimag(_out[n_vec]), lv_cimag(tmp)));
		}
	}

}
#endif /* LV_HAVE_SSE3 */

#ifdef LV_HAVE_SSE3
#include <pmmintrin.h>

/*!
 \brief Multiplies the reference complex vector with multiple versions of another complex vector, accumulates the results and stores them in the output vector
 \param[out] result        Array of num_a_vectors components with the multiple versions of in_a multiplied and accumulated The vector where the accumulated result will be stored
 \param[in]  in_common     Pointer to one of the vectors to be multiplied and accumulated (reference vector)
 \param[in]  in_a          Pointer to an array of pointers to multiple versions of the other vector to be multiplied and accumulated
 \param[in]  num_a_vectors Number of vectors to be multiplied by the reference vector and accumulated
 \param[in]  num_points    The Number of complex values to be multiplied together, accumulated and stored into result
 */
static inline void volk_gnsssdr_16ic_x2_rotator_dot_prod_16ic_xn_u_sse3(lv_16sc_t* out, const lv_16sc_t* in_common, const lv_32fc_t phase_inc, lv_32fc_t* phase, const lv_16sc_t** in_a,  int num_a_vectors, unsigned int num_points)
{
	lv_16sc_t dotProduct = lv_cmake(0,0);

	const unsigned int sse_iters = num_points / 4;

	const lv_16sc_t** _in_a = in_a;
	const lv_16sc_t* _in_common = in_common;
	lv_16sc_t* _out = out;

	__VOLK_ATTR_ALIGNED(16) lv_16sc_t dotProductVector[4];

	//todo dyn mem reg

	__m128i* realcacc;
	__m128i* imagcacc;

	realcacc = (__m128i*)calloc(num_a_vectors, sizeof(__m128i)); //calloc also sets memory to 0
	imagcacc = (__m128i*)calloc(num_a_vectors, sizeof(__m128i)); //calloc also sets memory to 0

	__m128i a,b,c, c_sr, mask_imag, mask_real, real, imag, imag1,imag2, b_sl, a_sl, result;

	mask_imag = _mm_set_epi8(255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0);
	mask_real = _mm_set_epi8(0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255);

	// phase rotation registers
	__m128 pa, pb, two_phase_acc_reg, two_phase_inc_reg;
	__m128i pc1, pc2;
	__attribute__((aligned(16))) lv_32fc_t two_phase_inc[2];
	two_phase_inc[0] = phase_inc * phase_inc;
	two_phase_inc[1] = phase_inc * phase_inc;
	two_phase_inc_reg = _mm_load_ps((float*) two_phase_inc);
	__attribute__((aligned(16))) lv_32fc_t two_phase_acc[2];
	two_phase_acc[0] = (*phase);
	two_phase_acc[1] = (*phase) * phase_inc;
	two_phase_acc_reg = _mm_load_ps((float*)two_phase_acc);
	__m128 yl, yh, tmp1, tmp2, tmp3;
	lv_16sc_t tmp16;
	lv_32fc_t tmp32;

	for(unsigned int number = 0; number < sse_iters; number++)
	{
		// Phase rotation on operand in_common starts here:

		pa = _mm_set_ps((float)(lv_cimag(_in_common[1])), (float)(lv_creal(_in_common[1])), (float)(lv_cimag(_in_common[0])), (float)(lv_creal(_in_common[0]))); // //load (2 byte imag, 2 byte real) x 2 into 128 bits reg
		//complex 32fc multiplication b=a*two_phase_acc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(pa, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		pa = _mm_shuffle_ps(pa, pa, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(pa, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		pb = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di
		pc1 = _mm_cvtps_epi32(pb); // convert from 32fc to 32ic

		//complex 32fc multiplication two_phase_acc_reg=two_phase_acc_reg*two_phase_inc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(two_phase_inc_reg, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		tmp3 = _mm_shuffle_ps(two_phase_inc_reg, two_phase_inc_reg, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(tmp3, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		two_phase_acc_reg = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di

		//next two samples
		_in_common += 2;
		pa = _mm_set_ps((float)(lv_cimag(_in_common[1])), (float)(lv_creal(_in_common[1])), (float)(lv_cimag(_in_common[0])), (float)(lv_creal(_in_common[0]))); // //load (2 byte imag, 2 byte real) x 2 into 128 bits reg
		//complex 32fc multiplication b=a*two_phase_acc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(pa, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		pa = _mm_shuffle_ps(pa, pa, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(pa, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		pb = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di
		pc2 = _mm_cvtps_epi32(pb); // convert from 32fc to 32ic

		//complex 32fc multiplication two_phase_acc_reg=two_phase_acc_reg*two_phase_inc_reg
		yl = _mm_moveldup_ps(two_phase_acc_reg); // Load yl with cr,cr,dr,dr
		yh = _mm_movehdup_ps(two_phase_acc_reg); // Load yh with ci,ci,di,di
		tmp1 = _mm_mul_ps(two_phase_inc_reg, yl); // tmp1 = ar*cr,ai*cr,br*dr,bi*dr
		tmp3 = _mm_shuffle_ps(two_phase_inc_reg, two_phase_inc_reg, 0xB1); // Re-arrange x to be ai,ar,bi,br
		tmp2 = _mm_mul_ps(tmp3, yh); // tmp2 = ai*ci,ar*ci,bi*di,br*di
		two_phase_acc_reg = _mm_addsub_ps(tmp1, tmp2); // ar*cr-ai*ci, ai*cr+ar*ci, br*dr-bi*di, bi*dr+br*di

		// store four rotated in_common samples in the register b
		b = _mm_packs_epi32(pc1, pc2);// convert from 32ic to 16ic

		//next two samples
		_in_common += 2;

		for (int n_vec = 0; n_vec < num_a_vectors; n_vec++)
		{
			a = _mm_loadu_si128((__m128i*)&(_in_a[n_vec][number*4])); //load (2 byte imag, 2 byte real) x 4 into 128 bits reg

			c = _mm_mullo_epi16 (a, b); // a3.i*b3.i, a3.r*b3.r, ....

			c_sr = _mm_srli_si128 (c, 2); // Shift a right by imm8 bytes while shifting in zeros, and store the results in dst.
			real = _mm_subs_epi16 (c, c_sr);

			b_sl = _mm_slli_si128(b, 2); // b3.r, b2.i ....
			a_sl = _mm_slli_si128(a, 2); // a3.r, a2.i ....

			imag1 = _mm_mullo_epi16(a, b_sl); // a3.i*b3.r, ....
			imag2 = _mm_mullo_epi16(b, a_sl); // b3.i*a3.r, ....

			imag = _mm_adds_epi16(imag1, imag2);

			realcacc[n_vec] = _mm_adds_epi16 (realcacc[n_vec], real);
			imagcacc[n_vec] = _mm_adds_epi16 (imagcacc[n_vec], imag);

		}
	}

	for (int n_vec=0;n_vec<num_a_vectors;n_vec++)
	{
		realcacc[n_vec] = _mm_and_si128 (realcacc[n_vec], mask_real);
		imagcacc[n_vec] = _mm_and_si128 (imagcacc[n_vec], mask_imag);

		result = _mm_or_si128 (realcacc[n_vec], imagcacc[n_vec]);

		_mm_storeu_si128((__m128i*)dotProductVector, result); // Store the results back into the dot product vector
		dotProduct = lv_cmake(0,0);
		for (int i = 0; i<4; ++i)
		{
			dotProduct = lv_cmake(sat_adds16i(lv_creal(dotProduct), lv_creal(dotProductVector[i])),
					sat_adds16i(lv_cimag(dotProduct), lv_cimag(dotProductVector[i])));
		}
		_out[n_vec] = dotProduct;
	}
	free(realcacc);
	free(imagcacc);

	_mm_store_ps((float*)two_phase_acc, two_phase_acc_reg);
	(*phase) = lv_cmake(two_phase_acc[0], two_phase_acc[0]);

	for (int n_vec = 0; n_vec < num_a_vectors; n_vec++)
	{
		for(unsigned int n  = sse_iters * 4; n < num_points; n++)
		{
			tmp16 = *in_common++;
			tmp32 = lv_cmake((float)lv_creal(tmp16), (float)lv_cimag(tmp16)) * (*phase);
			tmp16 = lv_cmake((int16_t)rintf(lv_creal(tmp32)), (int16_t)rintf(lv_cimag(tmp32)));
			(*phase) *= phase_inc;
			lv_16sc_t tmp = tmp16 * in_a[n_vec][n];
			_out[n_vec] = lv_cmake(sat_adds16i(lv_creal(_out[n_vec]), lv_creal(tmp)),
					sat_adds16i(lv_cimag(_out[n_vec]), lv_cimag(tmp)));
		}
	}

}
#endif /* LV_HAVE_SSE3 */
#endif /*INCLUDED_volk_gnsssdr_16ic_xn_dot_prod_16ic_xn_H*/