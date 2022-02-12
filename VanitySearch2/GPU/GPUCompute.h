/*
 * This file is part of the VanitySearch distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// CUDA Kernel main function
// Compute SecpK1 keys and calculate RIPEMD160(SHA256(key)) then check prefix
// For the kernel, we use a 16 bits prefix lookup table which correspond to ~3 Base58 characters
// A second level lookup table contains 32 bits prefix (if used)
// (The CPU computes the full address and check the full prefix)
//
// We use affine coordinates for elliptic curve point (ie Z=1)

__device__ __noinline__ void CheckPoint(uint32_t *_h, int32_t incr, int32_t endo, int32_t mode, prefix_t *prefix,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out, int type) {

	uint32_t   off;
	prefixl_t  l32;
	prefix_t   pr0;
	prefix_t   hit;
	uint32_t   pos;
	uint32_t   st;
	uint32_t   ed;
	uint32_t   mi;
	uint32_t   lmi;
	uint32_t   tid = (blockIdx.x*blockDim.x) + threadIdx.x;
	char       add[48];

	if (prefix == NULL) {

		// No lookup compute address and return
		char *pattern = (char *)lookup32;
		_GetAddress(type, _h, add);
		if (_Match(add, pattern)) {
			// found
			goto addItem;
		}

	}
	else {

		// Lookup table
		pr0 = *(prefix_t *)(_h);
		hit = prefix[pr0];

		if (hit) {

			if (lookup32) {
				off = lookup32[pr0];
				l32 = _h[0];
				st = off;
				ed = off + hit - 1;
				while (st <= ed) {
					mi = (st + ed) / 2;
					lmi = lookup32[mi];
					if (l32 < lmi) {
						ed = mi - 1;
					}
					else if (l32 == lmi) {
						// found
						goto addItem;
					}
					else {
						st = mi + 1;
					}
				}
				return;
			}

		addItem:

			pos = atomicAdd(out, 1);
			if (pos < maxFound) {
				out[pos*ITEM_SIZE32 + 1] = tid;
				out[pos*ITEM_SIZE32 + 2] = (uint32_t)(incr << 16) | (uint32_t)(mode << 15) | (uint32_t)(endo);
				out[pos*ITEM_SIZE32 + 3] = _h[0];
				out[pos*ITEM_SIZE32 + 4] = _h[1];
				out[pos*ITEM_SIZE32 + 5] = _h[2];
				out[pos*ITEM_SIZE32 + 6] = _h[3];
				out[pos*ITEM_SIZE32 + 7] = _h[4];
			}

		}

	}

}

__device__ __noinline__ void CheckPointETH(uint32_t *_h, int32_t incr, int32_t endo, int32_t mode, prefix_t *prefix,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out, int type) {

	uint32_t   off;
	prefixl_t  l32;
	prefix_t   pr0;
	prefix_t   hit;
	uint32_t   pos;
	uint32_t   st;
	uint32_t   ed;
	uint32_t   mi;
	uint32_t   lmi;
	uint32_t   tid = (blockIdx.x*blockDim.x) + threadIdx.x;
	char       add[43];

	if (prefix == NULL) {

		// No lookup compute address and return
		char *pattern = (char *)lookup32;
		_GetAddressETH(_h, add);
		if (_Match(add, pattern)) {
			// found
			goto addItem;
		}

		//printf("%s\n", add);

	}
	else {

		// Lookup table
		pr0 = *(prefix_t *)(_h);
		hit = prefix[pr0];

		/*		for (int i = 0; i < 20; i++) {
					printf("%02x", ((uint8_t*)_h)[i]);
				}
				printf("\n")*/;

		if (hit) {

			if (lookup32) {
				off = lookup32[pr0];
				l32 = _h[0];
				st = off;
				ed = off + hit - 1;
				while (st <= ed) {
					mi = (st + ed) / 2;
					lmi = lookup32[mi];
					if (l32 < lmi) {
						ed = mi - 1;
					}
					else if (l32 == lmi) {
						// found
						goto addItem;
					}
					else {
						st = mi + 1;
					}
				}
				return;
			}

		addItem:

			pos = atomicAdd(out, 1);
			if (pos < maxFound) {
				out[pos*ITEM_SIZE32 + 1] = tid;
				out[pos*ITEM_SIZE32 + 2] = (uint32_t)(incr << 16) | (uint32_t)(mode << 15) | (uint32_t)(endo);
				out[pos*ITEM_SIZE32 + 3] = _h[0];
				out[pos*ITEM_SIZE32 + 4] = _h[1];
				out[pos*ITEM_SIZE32 + 5] = _h[2];
				out[pos*ITEM_SIZE32 + 6] = _h[3];
				out[pos*ITEM_SIZE32 + 7] = _h[4];
			}

		}

	}

}

// -----------------------------------------------------------------------------------------

#define CHECK_POINT(_h,incr,endo,mode)  CheckPoint(_h,incr,endo,mode,prefix,lookup32,maxFound,out,P2PKH)
#define CHECK_POINT_P2SH(_h,incr,endo,mode)  CheckPoint(_h,incr,endo,mode,prefix,lookup32,maxFound,out,P2SH)

__device__ __noinline__ void CheckHashComp256(prefix_t *prefix, uint64_t *px, uint8_t isOdd, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];
	uint64_t   pe1x[4];
	uint64_t   pe2x[4];

	_GetHash160Comp(px, isOdd, (uint8_t *)h);
	CHECK_POINT(h, incr, 0, true);
	_ModMult(pe1x, px, _beta);
	_GetHash160Comp(pe1x, isOdd, (uint8_t *)h);
	CHECK_POINT(h, incr, 1, true);
	_ModMult(pe2x, px, _beta2);
	_GetHash160Comp(pe2x, isOdd, (uint8_t *)h);
	CHECK_POINT(h, incr, 2, true);

	_GetHash160Comp(px, !isOdd, (uint8_t *)h);
	CHECK_POINT(h, -incr, 0, true);
	_GetHash160Comp(pe1x, !isOdd, (uint8_t *)h);
	CHECK_POINT(h, -incr, 1, true);
	_GetHash160Comp(pe2x, !isOdd, (uint8_t *)h);
	CHECK_POINT(h, -incr, 2, true);

}

__device__ __noinline__ void CheckHashCompB(prefix_t *prefix, uint64_t *px, uint8_t isOdd, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];

	_GetHash160Comp(px, isOdd, (uint8_t *)h);
	CHECK_POINT(h, incr, 0, true);

}

__device__ __noinline__ void CheckHashP2SHComp256(prefix_t *prefix, uint64_t *px, uint8_t isOdd, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];
	uint64_t   pe1x[4];
	uint64_t   pe2x[4];

	_GetHash160P2SHComp(px, isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 0, true);
	_ModMult(pe1x, px, _beta);
	_GetHash160P2SHComp(pe1x, isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 1, true);
	_ModMult(pe2x, px, _beta2);
	_GetHash160P2SHComp(pe2x, isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 2, true);

	_GetHash160P2SHComp(px, !isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, -incr, 0, true);
	_GetHash160P2SHComp(pe1x, !isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, -incr, 1, true);
	_GetHash160P2SHComp(pe2x, !isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, -incr, 2, true);

}

__device__ __noinline__ void CheckHashP2SHCompB(prefix_t *prefix, uint64_t *px, uint8_t isOdd, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];

	_GetHash160P2SHComp(px, isOdd, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 0, true);

}

// -----------------------------------------------------------------------------------------

__device__ __noinline__ void CheckHashUncomp256(prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];
	uint64_t   pe1x[4];
	uint64_t   pe2x[4];
	uint64_t   pyn[4];

	_GetHash160(px, py, (uint8_t *)h);
	CHECK_POINT(h, incr, 0, false);
	_ModMult(pe1x, px, _beta);
	_GetHash160(pe1x, py, (uint8_t *)h);
	CHECK_POINT(h, incr, 1, false);
	_ModMult(pe2x, px, _beta2);
	_GetHash160(pe2x, py, (uint8_t *)h);
	CHECK_POINT(h, incr, 2, false);

	ModNeg256(pyn, py);

	_GetHash160(px, pyn, (uint8_t *)h);
	CHECK_POINT(h, -incr, 0, false);
	_GetHash160(pe1x, pyn, (uint8_t *)h);
	CHECK_POINT(h, -incr, 1, false);
	_GetHash160(pe2x, pyn, (uint8_t *)h);
	CHECK_POINT(h, -incr, 2, false);

}

__device__ __noinline__ void CheckHashUncompB(prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];

	_GetHash160(px, py, (uint8_t *)h);
	CHECK_POINT(h, incr, 0, false);

}

__device__ __noinline__ void CheckHashP2SHUncomp256(prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];
	uint64_t   pe1x[4];
	uint64_t   pe2x[4];
	uint64_t   pyn[4];

	_GetHash160P2SHUncomp(px, py, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 0, false);
	_ModMult(pe1x, px, _beta);
	_GetHash160P2SHUncomp(pe1x, py, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 1, false);
	_ModMult(pe2x, px, _beta2);
	_GetHash160P2SHUncomp(pe2x, py, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 2, false);

	ModNeg256(pyn, py);

	_GetHash160P2SHUncomp(px, pyn, (uint8_t *)h);
	CHECK_POINT_P2SH(h, -incr, 0, false);
	_GetHash160P2SHUncomp(pe1x, pyn, (uint8_t *)h);
	CHECK_POINT_P2SH(h, -incr, 1, false);
	_GetHash160P2SHUncomp(pe2x, pyn, (uint8_t *)h);
	CHECK_POINT_P2SH(h, -incr, 2, false);

}

__device__ __noinline__ void CheckHashP2SHUncompB(prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];

	_GetHash160P2SHUncomp(px, py, (uint8_t *)h);
	CHECK_POINT_P2SH(h, incr, 0, false);

}

// -----------------------------------------------------------------------------------------

__device__ __noinline__ void CheckHash256(uint32_t mode, prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	switch (mode) {
	case SEARCH_COMPRESSED:
		CheckHashComp256(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		break;
	case SEARCH_UNCOMPRESSED:
		CheckHashUncomp256(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	case SEARCH_BOTH:
		CheckHashComp256(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		CheckHashUncomp256(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	}

}

__device__ __noinline__ void CheckHashB(uint32_t mode, prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	switch (mode) {
	case SEARCH_COMPRESSED:
		CheckHashCompB(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		break;
	case SEARCH_UNCOMPRESSED:
		CheckHashUncompB(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	case SEARCH_BOTH:
		CheckHashCompB(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		CheckHashUncompB(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	}

}

#define CHECK_POINT_ETH(_h,incr,endo,mode)  CheckPointETH(_h,incr,endo,mode,prefix,lookup32,maxFound,out,P2PKH)

__device__ __noinline__ void CheckHashETH256(uint32_t mode, prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];
	uint64_t   pe1x[4];
	uint64_t   pe2x[4];
	uint64_t   pyn[4];

	_GetHashKeccak160(px, py, h);
	CHECK_POINT_ETH(h, incr, 0, false);
	_ModMult(pe1x, px, _beta);
	_GetHashKeccak160(pe1x, py, h);
	CHECK_POINT_ETH(h, incr, 1, false);
	_ModMult(pe2x, px, _beta2);
	_GetHashKeccak160(pe2x, py, h);
	CHECK_POINT_ETH(h, incr, 2, false);

	ModNeg256(pyn, py);

	_GetHashKeccak160(px, pyn, h);
	CHECK_POINT_ETH(h, -incr, 0, false);
	_GetHashKeccak160(pe1x, pyn, h);
	CHECK_POINT_ETH(h, -incr, 1, false);
	_GetHashKeccak160(pe2x, pyn, h);
	CHECK_POINT_ETH(h, -incr, 2, false);
}


__device__ __noinline__ void CheckHashETHB(uint32_t mode, prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint32_t   h[5];
	_GetHashKeccak160(px, py, h);
	CHECK_POINT_ETH(h, incr, 0, false);
}


__device__ __noinline__ void CheckP2SHHash256(uint32_t mode, prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	switch (mode) {
	case SEARCH_COMPRESSED:
		CheckHashP2SHComp256(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		break;
	case SEARCH_UNCOMPRESSED:
		CheckHashP2SHUncomp256(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	case SEARCH_BOTH:
		CheckHashP2SHComp256(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		CheckHashP2SHUncomp256(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	}

}

__device__ __noinline__ void CheckP2SHHashB(uint32_t mode, prefix_t *prefix, uint64_t *px, uint64_t *py, int32_t incr,
	uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	switch (mode) {
	case SEARCH_COMPRESSED:
		CheckHashP2SHCompB(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		break;
	case SEARCH_UNCOMPRESSED:
		CheckHashP2SHUncompB(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	case SEARCH_BOTH:
		CheckHashP2SHCompB(prefix, px, (uint8_t)(py[0] & 1), incr, lookup32, maxFound, out);
		CheckHashP2SHUncompB(prefix, px, py, incr, lookup32, maxFound, out);
		break;
	}

}

#define CHECK_PREFIX_256(incr) CheckHash256(mode, sPrefix, px, py, j*GRP_SIZE + (incr), lookup32, maxFound, out)
#define CHECK_PREFIX_B(incr) CheckHashB(mode, sPrefix, px, py, j*GRP_SIZE + (incr), lookup32, maxFound, out)

// -----------------------------------------------------------------------------------------

__device__ void ComputeKeys256(uint32_t mode, uint64_t *startx, uint64_t *starty,
	prefix_t *sPrefix, uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint64_t dx[GRP_SIZE / 2 + 1][4];
	uint64_t px[4];
	uint64_t py[4];
	uint64_t pyn[4];
	uint64_t sx[4];
	uint64_t sy[4];
	uint64_t dy[4];
	uint64_t _s[4];
	uint64_t _p2[4];
	char pattern[48];

	// Load starting key
	__syncthreads();
	Load256A(sx, startx);
	Load256A(sy, starty);
	Load256(px, sx);
	Load256(py, sy);

	if (sPrefix == NULL) {
		memcpy(pattern, lookup32, 48);
		lookup32 = (uint32_t *)pattern;
	}

	for (uint32_t j = 0; j < STEP_SIZE / GRP_SIZE; j++) {

		// Fill group with delta x
		uint32_t i;
		for (i = 0; i < HSIZE; i++)
			ModSub256(dx[i], Gx[i], sx);
		ModSub256(dx[i], Gx[i], sx);  // For the first point
		ModSub256(dx[i + 1], _2Gnx, sx);  // For the next center point

		// Compute modular inverse
		_ModInvGrouped(dx);

		// We use the fact that P + i*G and P - i*G has the same deltax, so the same inverse
		// We compute key in the positive and negative way from the center of the group

		// Check starting point
		CHECK_PREFIX_256(GRP_SIZE / 2);

		ModNeg256(pyn, py);

		for (i = 0; i < HSIZE; i++) {

			// P = StartPoint + i*G
			Load256(px, sx);
			Load256(py, sy);
			ModSub256(dy, Gy[i], py);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p2 = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, Gx[i], px);
			_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
			ModSub256(py, Gy[i]);         // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_256(GRP_SIZE / 2 + (i + 1));

			// P = StartPoint - i*G, if (x,y) = i*G then (x,-y) = -i*G
			Load256(px, sx);
			ModSub256(dy, pyn, Gy[i]);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, px, Gx[i]);
			_ModMult(py, _s);             // py = s*(ret.x-p2.x)
			ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_256(GRP_SIZE / 2 - (i + 1));

		}

		// First point (startP - (GRP_SZIE/2)*G)
		Load256(px, sx);
		Load256(py, sy);
		ModNeg256(dy, Gy[i]);
		ModSub256(dy, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);              // _p = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, px, Gx[i]);
		_ModMult(py, _s);             // py = s*(ret.x-p2.x)
		ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

		CHECK_PREFIX_256(0);

		i++;

		// Next start point (startP + GRP_SIZE*G)
		Load256(px, sx);
		Load256(py, sy);
		ModSub256(dy, _2Gny, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);             // _p2 = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, _2Gnx);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, _2Gnx, px);
		_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
		ModSub256(py, _2Gny);         // py = - p2.y - s*(ret.x-p2.x);

	}

	// Update starting point
	__syncthreads();
	Store256A(startx, px);
	Store256A(starty, py);

}

__device__ void ComputeKeysB(uint32_t mode, uint64_t *startx, uint64_t *starty,
	prefix_t *sPrefix, uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint64_t dx[GRP_SIZE / 2 + 1][4];
	uint64_t px[4];
	uint64_t py[4];
	uint64_t pyn[4];
	uint64_t sx[4];
	uint64_t sy[4];
	uint64_t dy[4];
	uint64_t _s[4];
	uint64_t _p2[4];
	char pattern[48];

	// Load starting key
	__syncthreads();
	Load256A(sx, startx);
	Load256A(sy, starty);
	Load256(px, sx);
	Load256(py, sy);

	if (sPrefix == NULL) {
		memcpy(pattern, lookup32, 48);
		lookup32 = (uint32_t *)pattern;
	}

	for (uint32_t j = 0; j < STEP_SIZE / GRP_SIZE; j++) {

		// Fill group with delta x
		uint32_t i;
		for (i = 0; i < HSIZE; i++)
			ModSub256(dx[i], Gx[i], sx);
		ModSub256(dx[i], Gx[i], sx);  // For the first point
		ModSub256(dx[i + 1], _2Gnx, sx);  // For the next center point

		// Compute modular inverse
		_ModInvGrouped(dx);

		// We use the fact that P + i*G and P - i*G has the same deltax, so the same inverse
		// We compute key in the positive and negative way from the center of the group

		// Check starting point
		CHECK_PREFIX_B(GRP_SIZE / 2);

		ModNeg256(pyn, py);

		for (i = 0; i < HSIZE; i++) {

			// P = StartPoint + i*G
			Load256(px, sx);
			Load256(py, sy);
			ModSub256(dy, Gy[i], py);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p2 = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, Gx[i], px);
			_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
			ModSub256(py, Gy[i]);         // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_B(GRP_SIZE / 2 + (i + 1));

			// P = StartPoint - i*G, if (x,y) = i*G then (x,-y) = -i*G
			Load256(px, sx);
			ModSub256(dy, pyn, Gy[i]);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, px, Gx[i]);
			_ModMult(py, _s);             // py = s*(ret.x-p2.x)
			ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_B(GRP_SIZE / 2 - (i + 1));

		}

		// First point (startP - (GRP_SZIE/2)*G)
		Load256(px, sx);
		Load256(py, sy);
		ModNeg256(dy, Gy[i]);
		ModSub256(dy, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);              // _p = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, px, Gx[i]);
		_ModMult(py, _s);             // py = s*(ret.x-p2.x)
		ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

		CHECK_PREFIX_B(0);

		i++;

		// Next start point (startP + GRP_SIZE*G)
		Load256(px, sx);
		Load256(py, sy);
		ModSub256(dy, _2Gny, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);             // _p2 = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, _2Gnx);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, _2Gnx, px);
		_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
		ModSub256(py, _2Gny);         // py = - p2.y - s*(ret.x-p2.x);

	}

	// Update starting point
	__syncthreads();
	Store256A(startx, px);
	Store256A(starty, py);

}

// -----------------------------------------------------------------------------------------

#define CHECK_PREFIX_ETH_256(incr) CheckHashETH256(mode, sPrefix, px, py, j*GRP_SIZE + (incr), lookup32, maxFound, out)
#define CHECK_PREFIX_ETH_B(incr) CheckHashETHB(mode, sPrefix, px, py, j*GRP_SIZE + (incr), lookup32, maxFound, out)

// -----------------------------------------------------------------------------------------

__device__ void ComputeKeysETH256(uint32_t mode, uint64_t *startx, uint64_t *starty,
	prefix_t *sPrefix, uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint64_t dx[GRP_SIZE / 2 + 1][4];
	uint64_t px[4];
	uint64_t py[4];
	uint64_t pyn[4];
	uint64_t sx[4];
	uint64_t sy[4];
	uint64_t dy[4];
	uint64_t _s[4];
	uint64_t _p2[4];
	char pattern[48];

	// Load starting key
	__syncthreads();
	Load256A(sx, startx);
	Load256A(sy, starty);
	Load256(px, sx);
	Load256(py, sy);

	if (sPrefix == NULL) {
		memcpy(pattern, lookup32, 48);
		lookup32 = (uint32_t *)pattern;
	}

	for (uint32_t j = 0; j < STEP_SIZE / GRP_SIZE; j++) {

		// Fill group with delta x
		uint32_t i;
		for (i = 0; i < HSIZE; i++)
			ModSub256(dx[i], Gx[i], sx);
		ModSub256(dx[i], Gx[i], sx);  // For the first point
		ModSub256(dx[i + 1], _2Gnx, sx);  // For the next center point

		// Compute modular inverse
		_ModInvGrouped(dx);

		// We use the fact that P + i*G and P - i*G has the same deltax, so the same inverse
		// We compute key in the positive and negative way from the center of the group

		// Check starting point
		CHECK_PREFIX_ETH_256(GRP_SIZE / 2);

		ModNeg256(pyn, py);

		for (i = 0; i < HSIZE; i++) {

			// P = StartPoint + i*G
			Load256(px, sx);
			Load256(py, sy);
			ModSub256(dy, Gy[i], py);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p2 = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, Gx[i], px);
			_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
			ModSub256(py, Gy[i]);         // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_ETH_256(GRP_SIZE / 2 + (i + 1));

			// P = StartPoint - i*G, if (x,y) = i*G then (x,-y) = -i*G
			Load256(px, sx);
			ModSub256(dy, pyn, Gy[i]);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, px, Gx[i]);
			_ModMult(py, _s);             // py = s*(ret.x-p2.x)
			ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_ETH_256(GRP_SIZE / 2 - (i + 1));

		}

		// First point (startP - (GRP_SZIE/2)*G)
		Load256(px, sx);
		Load256(py, sy);
		ModNeg256(dy, Gy[i]);
		ModSub256(dy, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);              // _p = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, px, Gx[i]);
		_ModMult(py, _s);             // py = s*(ret.x-p2.x)
		ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

		CHECK_PREFIX_ETH_256(0);

		i++;

		// Next start point (startP + GRP_SIZE*G)
		Load256(px, sx);
		Load256(py, sy);
		ModSub256(dy, _2Gny, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);             // _p2 = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, _2Gnx);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, _2Gnx, px);
		_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
		ModSub256(py, _2Gny);         // py = - p2.y - s*(ret.x-p2.x);

	}

	// Update starting point
	__syncthreads();
	Store256A(startx, px);
	Store256A(starty, py);

}

__device__ void ComputeKeysETHB(uint32_t mode, uint64_t *startx, uint64_t *starty,
	prefix_t *sPrefix, uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint64_t dx[GRP_SIZE / 2 + 1][4];
	uint64_t px[4];
	uint64_t py[4];
	uint64_t pyn[4];
	uint64_t sx[4];
	uint64_t sy[4];
	uint64_t dy[4];
	uint64_t _s[4];
	uint64_t _p2[4];
	char pattern[48];

	// Load starting key
	__syncthreads();
	Load256A(sx, startx);
	Load256A(sy, starty);
	Load256(px, sx);
	Load256(py, sy);

	if (sPrefix == NULL) {
		memcpy(pattern, lookup32, 48);
		lookup32 = (uint32_t *)pattern;
	}

	for (uint32_t j = 0; j < STEP_SIZE / GRP_SIZE; j++) {

		// Fill group with delta x
		uint32_t i;
		for (i = 0; i < HSIZE; i++)
			ModSub256(dx[i], Gx[i], sx);
		ModSub256(dx[i], Gx[i], sx);  // For the first point
		ModSub256(dx[i + 1], _2Gnx, sx);  // For the next center point

		// Compute modular inverse
		_ModInvGrouped(dx);

		// We use the fact that P + i*G and P - i*G has the same deltax, so the same inverse
		// We compute key in the positive and negative way from the center of the group

		// Check starting point
		CHECK_PREFIX_ETH_B(GRP_SIZE / 2);

		ModNeg256(pyn, py);

		for (i = 0; i < HSIZE; i++) {

			// P = StartPoint + i*G
			Load256(px, sx);
			Load256(py, sy);
			ModSub256(dy, Gy[i], py);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p2 = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, Gx[i], px);
			_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
			ModSub256(py, Gy[i]);         // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_ETH_B(GRP_SIZE / 2 + (i + 1));

			// P = StartPoint - i*G, if (x,y) = i*G then (x,-y) = -i*G
			Load256(px, sx);
			ModSub256(dy, pyn, Gy[i]);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, px, Gx[i]);
			_ModMult(py, _s);             // py = s*(ret.x-p2.x)
			ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_ETH_B(GRP_SIZE / 2 - (i + 1));

		}

		// First point (startP - (GRP_SZIE/2)*G)
		Load256(px, sx);
		Load256(py, sy);
		ModNeg256(dy, Gy[i]);
		ModSub256(dy, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);              // _p = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, px, Gx[i]);
		_ModMult(py, _s);             // py = s*(ret.x-p2.x)
		ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

		CHECK_PREFIX_ETH_B(0);

		i++;

		// Next start point (startP + GRP_SIZE*G)
		Load256(px, sx);
		Load256(py, sy);
		ModSub256(dy, _2Gny, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);             // _p2 = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, _2Gnx);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, _2Gnx, px);
		_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
		ModSub256(py, _2Gny);         // py = - p2.y - s*(ret.x-p2.x);

	}

	// Update starting point
	__syncthreads();
	Store256A(startx, px);
	Store256A(starty, py);

}

// -----------------------------------------------------------------------------------------

#define CHECK_PREFIX_P2SH_256(incr) CheckP2SHHash256(mode, sPrefix, px, py, j*GRP_SIZE + (incr), lookup32, maxFound, out)
#define CHECK_PREFIX_P2SH_B(incr) CheckP2SHHashB(mode, sPrefix, px, py, j*GRP_SIZE + (incr), lookup32, maxFound, out)

__device__ void ComputeKeysP2SH256(uint32_t mode, uint64_t *startx, uint64_t *starty,
	prefix_t *sPrefix, uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint64_t dx[GRP_SIZE / 2 + 1][4];
	uint64_t px[4];
	uint64_t py[4];
	uint64_t pyn[4];
	uint64_t sx[4];
	uint64_t sy[4];
	uint64_t dy[4];
	uint64_t _s[4];
	uint64_t _p2[4];
	char pattern[48];

	// Load starting key
	__syncthreads();
	Load256A(sx, startx);
	Load256A(sy, starty);
	Load256(px, sx);
	Load256(py, sy);

	if (sPrefix == NULL) {
		memcpy(pattern, lookup32, 48);
		lookup32 = (uint32_t *)pattern;
	}

	for (uint32_t j = 0; j < STEP_SIZE / GRP_SIZE; j++) {

		// Fill group with delta x
		uint32_t i;
		for (i = 0; i < HSIZE; i++)
			ModSub256(dx[i], Gx[i], sx);
		ModSub256(dx[i], Gx[i], sx);  // For the first point
		ModSub256(dx[i + 1], _2Gnx, sx);  // For the next center point

		// Compute modular inverse
		_ModInvGrouped(dx);

		// We use the fact that P + i*G and P - i*G has the same deltax, so the same inverse
		// We compute key in the positive and negative way from the center of the group

		// Check starting point
		CHECK_PREFIX_P2SH_256(GRP_SIZE / 2);

		ModNeg256(pyn, py);

		for (i = 0; i < HSIZE; i++) {

			__syncthreads();
			// P = StartPoint + i*G
			Load256(px, sx);
			Load256(py, sy);
			ModSub256(dy, Gy[i], py);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p2 = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, Gx[i], px);
			_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
			ModSub256(py, Gy[i]);         // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_P2SH_256(GRP_SIZE / 2 + (i + 1));

			__syncthreads();
			// P = StartPoint - i*G, if (x,y) = i*G then (x,-y) = -i*G
			Load256(px, sx);
			ModSub256(dy, pyn, Gy[i]);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, px, Gx[i]);
			_ModMult(py, _s);             // py = s*(ret.x-p2.x)
			ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_P2SH_256(GRP_SIZE / 2 - (i + 1));

		}

		__syncthreads();
		// First point (startP - (GRP_SZIE/2)*G)
		Load256(px, sx);
		Load256(py, sy);
		ModNeg256(dy, Gy[i]);
		ModSub256(dy, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);              // _p = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, px, Gx[i]);
		_ModMult(py, _s);             // py = s*(ret.x-p2.x)
		ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

		CHECK_PREFIX_P2SH_256(0);

		i++;

		__syncthreads();
		// Next start point (startP + GRP_SIZE*G)
		Load256(px, sx);
		Load256(py, sy);
		ModSub256(dy, _2Gny, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);             // _p2 = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, _2Gnx);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, _2Gnx, px);
		_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
		ModSub256(py, _2Gny);         // py = - p2.y - s*(ret.x-p2.x);

	}

	// Update starting point
	__syncthreads();
	Store256A(startx, px);
	Store256A(starty, py);

}

__device__ void ComputeKeysP2SHB(uint32_t mode, uint64_t *startx, uint64_t *starty,
	prefix_t *sPrefix, uint32_t *lookup32, uint32_t maxFound, uint32_t *out) {

	uint64_t dx[GRP_SIZE / 2 + 1][4];
	uint64_t px[4];
	uint64_t py[4];
	uint64_t pyn[4];
	uint64_t sx[4];
	uint64_t sy[4];
	uint64_t dy[4];
	uint64_t _s[4];
	uint64_t _p2[4];
	char pattern[48];

	// Load starting key
	__syncthreads();
	Load256A(sx, startx);
	Load256A(sy, starty);
	Load256(px, sx);
	Load256(py, sy);

	if (sPrefix == NULL) {
		memcpy(pattern, lookup32, 48);
		lookup32 = (uint32_t *)pattern;
	}

	for (uint32_t j = 0; j < STEP_SIZE / GRP_SIZE; j++) {

		// Fill group with delta x
		uint32_t i;
		for (i = 0; i < HSIZE; i++)
			ModSub256(dx[i], Gx[i], sx);
		ModSub256(dx[i], Gx[i], sx);  // For the first point
		ModSub256(dx[i + 1], _2Gnx, sx);  // For the next center point

		// Compute modular inverse
		_ModInvGrouped(dx);

		// We use the fact that P + i*G and P - i*G has the same deltax, so the same inverse
		// We compute key in the positive and negative way from the center of the group

		// Check starting point
		CHECK_PREFIX_P2SH_B(GRP_SIZE / 2);

		ModNeg256(pyn, py);

		for (i = 0; i < HSIZE; i++) {

			__syncthreads();
			// P = StartPoint + i*G
			Load256(px, sx);
			Load256(py, sy);
			ModSub256(dy, Gy[i], py);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p2 = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, Gx[i], px);
			_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
			ModSub256(py, Gy[i]);         // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_P2SH_B(GRP_SIZE / 2 + (i + 1));

			__syncthreads();
			// P = StartPoint - i*G, if (x,y) = i*G then (x,-y) = -i*G
			Load256(px, sx);
			ModSub256(dy, pyn, Gy[i]);

			_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
			_ModSqr(_p2, _s);             // _p = pow2(s)

			ModSub256(px, _p2, px);
			ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

			ModSub256(py, px, Gx[i]);
			_ModMult(py, _s);             // py = s*(ret.x-p2.x)
			ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

			CHECK_PREFIX_P2SH_B(GRP_SIZE / 2 - (i + 1));

		}

		__syncthreads();
		// First point (startP - (GRP_SZIE/2)*G)
		Load256(px, sx);
		Load256(py, sy);
		ModNeg256(dy, Gy[i]);
		ModSub256(dy, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);              // _p = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, Gx[i]);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, px, Gx[i]);
		_ModMult(py, _s);             // py = s*(ret.x-p2.x)
		ModSub256(py, Gy[i], py);     // py = - p2.y - s*(ret.x-p2.x);

		CHECK_PREFIX_P2SH_B(0);

		i++;

		__syncthreads();
		// Next start point (startP + GRP_SIZE*G)
		Load256(px, sx);
		Load256(py, sy);
		ModSub256(dy, _2Gny, py);

		_ModMult(_s, dy, dx[i]);      //  s = (p2.y-p1.y)*inverse(p2.x-p1.x)
		_ModSqr(_p2, _s);             // _p2 = pow2(s)

		ModSub256(px, _p2, px);
		ModSub256(px, _2Gnx);         // px = pow2(s) - p1.x - p2.x;

		ModSub256(py, _2Gnx, px);
		_ModMult(py, _s);             // py = - s*(ret.x-p2.x)
		ModSub256(py, _2Gny);         // py = - p2.y - s*(ret.x-p2.x);

	}

	// Update starting point
	__syncthreads();
	Store256A(startx, px);
	Store256A(starty, py);

}
