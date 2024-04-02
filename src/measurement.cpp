#include <assert.h>
#include "measurement.h"

Measurement::Measurement(torch::Tensor& aT, torch::Tensor& eT, torch::Tensor& cT, Model model) : aT(aT), eT(eT), cT(cT), model(model){
	buildHam(model);
}

void Measurement::buildHam(Model model) {
	if (model.model_name == "ising") {
		double hx = std::get<0>(model.h);
		double Jz = model.J;
		ham = -2.0 * Jz * torch::kron(sz, sz) - hx * (torch::kron(sx, i2) + torch::kron(i2, sx)) / 2.0;
	}
}

void Measurement::print_measurements() {
	assert(
		measurements.find("energy") != measurements.end() &&
		measurements.find("mx")		!= measurements.end() &&
		measurements.find("my")		!= measurements.end() &&
		measurements.find("mz")		!= measurements.end()
	);
	
	std::cout << "E, mx, my, mz = " 
		<< measurements["energy"] << ' '
		<< measurements["mx"] << ' '
		<< measurements["my"] << ' '
		<< measurements["mz"] << ' '
		<< std::endl;
}

torch::Tensor Measurement::measure() {
	torch::Tensor tdT = torch::einsum("mefgh,nabcd->eafbgchdmn", { aT, aT });
	tdT = tdT.contiguous().view({ aT.size(1) * aT.size(1), aT.size(2) * aT.size(2), aT.size(3) * aT.size(3), aT.size(4) * aT.size(4), aT.size(0), aT.size(0) });

	torch::Tensor ceT = torch::tensordot(cT, eT, { 1 }, { 0 });
	torch::Tensor rT = torch::tensordot(eT, ceT, { 2 }, { 0 });
	rT = torch::tensordot(rT, tdT, { 1, 2 }, { 1, 0 });
	rT = torch::tensordot(rT, ceT, { 0, 2 }, { 0, 1 });
	rT = torch::tensordot(rT, rT, { 0, 1, 4 }, { 0, 1, 4 });

	rT = rT.permute({ 0, 2, 1, 3 });
	rT = rT.contiguous().view({ aT.size(0) * aT.size(0), aT.size(0) * aT.size(0) });

	rT = (rT + rT.t()) / 2.0;
	double nT = rT.trace().item<double>();

	measurements["energy"] = torch::mm(rT, ham).trace().item<double>() / nT;
	measurements["mx"] = torch::mm(rT, torch::kron(sx, i2)).trace().item<double>() / nT;
	measurements["my"] = torch::mm(rT, torch::kron(sy, i2)).trace().item<double>() / nT;
	measurements["mz"] = torch::mm(rT, torch::kron(sz, i2)).trace().item<double>() / nT;

	return torch::mm(rT, ham).trace() / rT.trace();
}