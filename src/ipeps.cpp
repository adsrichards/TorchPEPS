#include <iostream>
#include <algorithm>

#include "ipeps.h"
#include "utils.h"

Ipeps::Ipeps(int pDim, int bDim, int cDim, int rSteps) : pDim(pDim), bDim(bDim), cDim(cDim), rSteps(rSteps){
	aT = torch::rand({ pDim, bDim, bDim, bDim, bDim });
	aT = aT / torch::norm(aT).item<double>();
}

Ipeps::~Ipeps() {}

inline double tNorm(torch::Tensor tT) {
	return torch::norm(tT).item<double>();
}

double Ipeps::forward() {
	symmetrize(aT);

	torch::Tensor tT = torch::mm(aT.view({ pDim, -1 }).t(), aT.view({ pDim, -1 }));
	tT = tT.contiguous().view({ bDim, bDim, bDim, bDim, bDim, bDim, bDim, bDim });

	tT = tT.permute({ 0, 4, 1, 5, 2, 6, 3, 7 });
	tT = tT.contiguous().view({ bDim * bDim, bDim * bDim, bDim * bDim, bDim * bDim });
	
	tT = tT / tNorm(tT);

	ctmrg(tT);

	return 0.0;
}

void Ipeps::ctmrg(torch::Tensor& tT) {
	torch::Tensor cT = tT.sum(c10::IntArrayRef({ 0, 1 }));
	torch::Tensor eT = tT.sum(c10::IntArrayRef({ 1 }));

	eT = eT.permute({ 0, 2, 1 });

	renormalize(tT, cT, eT);

	for (int i = 0; i < rSteps; i++) {
		eT = eT / tNorm(eT);
		renormalize(tT, cT, eT);
	}
}

void Ipeps::renormalize(torch::Tensor& tT, torch::Tensor& cT, torch::Tensor& eT) {
	torch::Tensor rT = rho(tT, cT, eT);
	torch::Tensor pT = std::get<0>(torch::svd(rT));
	const int cDimNew = std::min((int)(eT.size(0) * tT.size(0)), cDim);

	std::cout << cDimNew << std::endl;

	pT = pT.narrow(1, 0, cDimNew);
	renormalizeCorner(cT, rT, pT);

	pT = pT.view({ eT.size(0), tT.size(0), cDimNew });
	renormalizeEdge(eT, tT, pT);
}

torch::Tensor Ipeps::rho(torch::Tensor& tT, torch::Tensor& cT, torch::Tensor& eT) {
	torch::Tensor rT = torch::tensordot(cT, eT, { 1 }, { 0 });
	rT = torch::tensordot(rT, eT, { 0 }, { 0 });
	rT = torch::tensordot(rT, tT, { 0, 2 }, { 0, 1 });
	rT = rT.permute({ 0, 3, 1, 2 });

	rT = rT.contiguous().view({ eT.size(0) * tT.size(0), eT.size(0) * tT.size(0) });
	rT = (rT + rT.t());
	rT = rT / tNorm(rT);

	return rT;
}

void Ipeps::renormalizeCorner(torch::Tensor& cT, torch::Tensor& rT, torch::Tensor& pT) {
	cT = torch::mm(rT, pT);
	cT = torch::mm(pT.t(), cT);
	cT = (cT + cT.t());
	cT = cT / tNorm(cT);
}

void Ipeps::renormalizeEdge(torch::Tensor& eT, torch::Tensor& tT, torch::Tensor& pT) {
	eT = torch::tensordot(eT, pT, { 0 }, { 0 });
	eT = torch::tensordot(eT, tT, { 0, 2 }, { 1, 0 });
	eT = torch::tensordot(eT, pT, { 0, 2 }, { 0, 1 });
	eT = (eT + eT.permute({ 2, 1, 0 }));
}