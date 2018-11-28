// Copyright (c) 2018 Johannes Delport
// This code is licensed under MIT license (see LICENSE for details)
#include "j_parser.h"

// Possible functions that can be called
std::string funcsArray[] = { "COS",  "SIN",  "TAN",   "ACOS",  "ASIN",  "ATAN",
							 "COSH", "SINH", "TANH",  "ACOSH", "ASINH", "ATANH",
							 "EXP",  "LOG",  "LOG10", "SQRT",  "CBRT" };
std::unordered_map<std::string, double> consts = {{"C",299792458},
												  {"MU0",12.566370614E-7},
												  {"EPS0",8.854187817E-12},
												  {"PI",3.141592653589793}};
std::vector<std::string> funcs(funcsArray,
	funcsArray +
	sizeof(funcsArray) / sizeof(std::string));

void
Parser::parse_expression(std::string expName, std::string expr, std::string subckt) {
	if (iFile.paramValues.paramMap.count("expName") != 0)
		Errors::parsing_errors(EXPRESSION_ARLEADY_DEFINED, expName);
	std::string expToEval = expr;
	std::vector<std::string> rpnQueue, rpnQueueCopy, opStack;
	std::vector<char> qType, qTypeCopy;
	std::string partToEval;
	int opLoc, popCount = 0;
	double result = 0.0;
	std::string stringSuffix = "";
	if(subckt != "") stringSuffix = "|" + subckt;
	while (!expToEval.empty()) {
		opLoc = expToEval.find_first_of("/*-+(){}[]^");
		if (expToEval[opLoc] == '-')
			if (opLoc != 0)
				if (expToEval[opLoc - 1] == 'E')
					opLoc = expToEval.find_first_of("/*-+(){}[]^", opLoc + 1);
		if (opLoc == 0)
			partToEval = expToEval.substr(0, opLoc + 1);
		else
			partToEval = expToEval.substr(0, opLoc);
		if (isdigit(partToEval[0])) {
			rpnQueue.push_back(Misc::precise_to_string(Misc::modifier(partToEval)));
			qType.push_back('V');
		}
		else if ((iFile.paramValues.paramMap.count(partToEval) != 0) ||
			(iFile.paramValues.paramMap.count(partToEval + stringSuffix) != 0)) {
			if (subckt != "") {
				rpnQueue.push_back(
					Misc::precise_to_string(iFile.paramValues.paramMap.at(
						partToEval + stringSuffix)));
				qType.push_back('V');
			}
			else {
				rpnQueue.push_back(Misc::precise_to_string(
					iFile.paramValues.paramMap.at(partToEval)));
				qType.push_back('V');
			}
		}
		else if ((iFile.paramValues.unparsedMap.count(partToEval) != 0) ||
			(iFile.paramValues.unparsedMap.count(partToEval + stringSuffix) != 0)) {
			if (subckt != "") {
				Parser::parse_expression(partToEval,
					iFile.paramValues.unparsedMap.at(partToEval + stringSuffix), subckt);
				rpnQueue.push_back(
					Misc::precise_to_string(iFile.paramValues.paramMap.at(
						partToEval + stringSuffix)));
				qType.push_back('V');
			}
			else {
				Parser::parse_expression(partToEval,
					iFile.paramValues.unparsedMap.at(partToEval), subckt);
				rpnQueue.push_back(Misc::precise_to_string(
					iFile.paramValues.paramMap.at(partToEval)));
				qType.push_back('V');
			}
		}
		else if (std::find(funcs.begin(), funcs.end(), partToEval) != funcs.end())
			opStack.push_back(partToEval);
		else if (partToEval.find_first_of("/*-+^") != std::string::npos) {
			while ((!opStack.empty()) &&
				(((prec_lvl(opStack.back()) == 4) ||
				(prec_lvl(opStack.back()) >= prec_lvl(partToEval))) &&
					(opStack.back().find_first_of("([{") == std::string::npos) &&
					(partToEval != "^"))) {
				rpnQueue.push_back(opStack.back());
				qType.push_back('O');
				opStack.pop_back();
			}
			opStack.push_back(partToEval);
		}
		else if (partToEval.find_first_of("([{") != std::string::npos)
			opStack.push_back(partToEval);
		else if (partToEval.find_first_of(")]}") != std::string::npos) {
			while ((!opStack.empty()) &&
				(opStack.back().find_first_of("([{") == std::string::npos)) {
				rpnQueue.push_back(opStack.back());
				qType.push_back('O');
				opStack.pop_back();
			}
			if ((!opStack.empty()) &&
				(opStack.back().find_first_of("([{") != std::string::npos))
				opStack.pop_back();
			else
				Errors::parsing_errors(MISMATCHED_PARENTHESIS, expr);
		}
		else
			Errors::parsing_errors(UNIDENTIFIED_PART, partToEval);
		if (opLoc == 0)
			expToEval = expToEval.substr(opLoc + 1);
		if (opLoc == -1)
			expToEval = "";
		else
			expToEval = expToEval.substr(opLoc);
	}
	if (expToEval.empty())
		while (!opStack.empty()) {
			if (opStack.back().find_first_of("([{") != std::string::npos)
				Errors::parsing_errors(MISMATCHED_PARENTHESIS, expr);
			else {
				rpnQueue.push_back(opStack.back());
				qType.push_back('O');
				opStack.pop_back();
			}
		}
	while (rpnQueue.size() > 1) {
		rpnQueueCopy.clear();
		qTypeCopy.clear();
		for (int i = 0; i < qType.size(); i++) {
			if (qType[i] == 'V') {
				rpnQueueCopy.push_back(rpnQueue[i]);
				qTypeCopy.push_back('V');
			}
			else if (qType[i] == 'O') {
				if (i == 0)
					Errors::parsing_errors(INVALID_RPN, expr);
				else if (i < 2) {
					rpnQueueCopy.pop_back();
					rpnQueueCopy.push_back(Misc::precise_to_string(parse_operator(
						rpnQueue[i], 0, Misc::modifier(rpnQueue[i - 1]), popCount)));
				}
				else {
					result = parse_operator(rpnQueue[i],
						Misc::modifier(rpnQueue[i - 2]),
						Misc::modifier(rpnQueue[i - 1]),
						popCount);
					for (int k = 0; k < popCount; k++)
						rpnQueueCopy.pop_back();
					if (popCount == 2)
						qTypeCopy.pop_back();
					rpnQueueCopy.push_back(Misc::precise_to_string(result));
				}
				if (rpnQueue.size() >= i) {
					rpnQueueCopy.insert(
						rpnQueueCopy.end(), rpnQueue.begin() + i + 1, rpnQueue.end());
					qTypeCopy.insert(qTypeCopy.end(), qType.begin() + i + 1, qType.end());
				}
				break;
			}
		}
		rpnQueue = rpnQueueCopy;
		qType = qTypeCopy;
	}
	if(expName.find("|" + subckt) != std::string::npos)
		iFile.paramValues.insertParam(expName, Misc::modifier(rpnQueue.back()));
	else 
		iFile.paramValues.insertParam(expName, Misc::modifier(rpnQueue.back()), subckt);
}

double
Parser::parse_return_expression(std::string expr, std::string subckt) {
	std::string expToEval = expr;
	std::vector<std::string> rpnQueue, rpnQueueCopy, opStack;
	std::vector<char> qType, qTypeCopy;
	std::string partToEval;
	int opLoc, popCount = 0;
	double result = 0.0;
	while (!expToEval.empty()) {
		opLoc = expToEval.find_first_of("/*-+(){}[]^");
		if (expToEval[opLoc] == '-')
			if (opLoc != 0)
				if (expToEval[opLoc - 1] == 'E')
					opLoc = expToEval.find_first_of("/*-+(){}[]^", opLoc + 1);
		if (opLoc == 0)
			partToEval = expToEval.substr(0, opLoc + 1);
		else
			partToEval = expToEval.substr(0, opLoc);
		if (isdigit(partToEval[0])) {
			rpnQueue.push_back(Misc::precise_to_string(Misc::modifier(partToEval)));
			qType.push_back('V');
		}
		else if (iFile.paramValues.paramMap.count(partToEval) != 0) {
			rpnQueue.push_back(Misc::precise_to_string(
				iFile.paramValues.paramMap.at(partToEval)));
			qType.push_back('V');
		}
		else if (subckt != "") {
			if (iFile.paramValues.paramMap.count(partToEval + "|" + subckt) != 0) {
				rpnQueue.push_back(Misc::precise_to_string(
					iFile.paramValues.paramMap.at(partToEval + "|" + subckt)));
				qType.push_back('V');
			}
		}
		else if (std::find(funcs.begin(), funcs.end(), partToEval) != funcs.end())
			opStack.push_back(partToEval);
		else if (consts.count(partToEval) != 0){
			rpnQueue.push_back(Misc::precise_to_string(consts[partToEval]));
			qType.push_back('V');
		}
		else if (partToEval.find_first_of("/*-+^") != std::string::npos) {
			while ((!opStack.empty()) &&
				(((prec_lvl(opStack.back()) == 4) ||
				(prec_lvl(opStack.back()) >= prec_lvl(partToEval))) &&
					(opStack.back().find_first_of("([{") == std::string::npos) &&
					(partToEval != "^"))) {
				rpnQueue.push_back(opStack.back());
				qType.push_back('O');
				opStack.pop_back();
			}
			opStack.push_back(partToEval);
		}
		else if (partToEval.find_first_of("([{") != std::string::npos)
			opStack.push_back(partToEval);
		else if (partToEval.find_first_of(")]}") != std::string::npos) {
			while ((!opStack.empty()) &&
				(opStack.back().find_first_of("([{") == std::string::npos)) {
				rpnQueue.push_back(opStack.back());
				qType.push_back('O');
				opStack.pop_back();
			}
			if ((!opStack.empty()) &&
				(opStack.back().find_first_of("([{") != std::string::npos))
				opStack.pop_back();
			else
				Errors::parsing_errors(MISMATCHED_PARENTHESIS, expr);
		}
		else
			Errors::parsing_errors(UNIDENTIFIED_PART, partToEval);
		if (opLoc == 0)
			expToEval = expToEval.substr(opLoc + 1);
		if (opLoc == -1)
			expToEval = "";
		else
			expToEval = expToEval.substr(opLoc);
	}
	if (expToEval.empty())
		while (!opStack.empty()) {
			if (opStack.back().find_first_of("([{") != std::string::npos)
				Errors::parsing_errors(MISMATCHED_PARENTHESIS, expr);
			else {
				rpnQueue.push_back(opStack.back());
				qType.push_back('O');
				opStack.pop_back();
			}
		}
	while (rpnQueue.size() > 1) {
		rpnQueueCopy.clear();
		qTypeCopy.clear();
		for (int i = 0; i < qType.size(); i++) {
			if (qType[i] == 'V') {
				rpnQueueCopy.push_back(rpnQueue[i]);
				qTypeCopy.push_back('V');
			}
			else if (qType[i] == 'O') {
				if (i == 0)
					Errors::parsing_errors(INVALID_RPN, expr);
				else if (i < 2) {
					rpnQueueCopy.pop_back();
					rpnQueueCopy.push_back(Misc::precise_to_string(parse_operator(
						rpnQueue[i], 0, Misc::modifier(rpnQueue[i - 1]), popCount)));
				}
				else {
					result = parse_operator(rpnQueue[i],
						Misc::modifier(rpnQueue[i - 2]),
						Misc::modifier(rpnQueue[i - 1]),
						popCount);
					for (int k = 0; k < popCount; k++)
						rpnQueueCopy.pop_back();
					if (popCount == 2)
						qTypeCopy.pop_back();
					rpnQueueCopy.push_back(Misc::precise_to_string(result));
				}
				if (rpnQueue.size() >= i) {
					rpnQueueCopy.insert(
						rpnQueueCopy.end(), rpnQueue.begin() + i + 1, rpnQueue.end());
					qTypeCopy.insert(qTypeCopy.end(), qType.begin() + i + 1, qType.end());
				}
				break;
			}
		}
		rpnQueue = rpnQueueCopy;
		qType = qTypeCopy;
	}
	return Misc::modifier(rpnQueue.back());
}

int
Parser::prec_lvl(std::string op)
{
	switch (op[0]) {
		// + and - are lowest level
	case '+':
	case '-':
		return 1;
		// * and / are higher level
	case '*':
	case '/':
		return 2;
		// ^ (pow) is highest level
	case '^':
		return 3;
		// default (functions) are max level
	default:
		return 4;
	}
	return 4;
}

double
Parser::parse_operator(std::string op, double val1, double val2, int& popCount)
{
	if (std::find(funcs.begin(), funcs.end(), op) != funcs.end()) {
		popCount = 1;
		if (op == "SIN")
			return sin(val2);
		else if (op == "COS")
			return cos(val2);
		else if (op == "TAN")
			return tan(val2);
		else if (op == "ASIN")
			return asin(val2);
		else if (op == "ACOS")
			return acos(val2);
		else if (op == "ATAN")
			return atan(val2);
		else if (op == "SINH")
			return sinh(val2);
		else if (op == "COSH")
			return cosh(val2);
		else if (op == "TANH")
			return tanh(val2);
		else if (op == "ASINH")
			return asinh(val2);
		else if (op == "ACOSH")
			return acosh(val2);
		else if (op == "ATANH")
			return atanh(val2);
		else if (op == "EXP")
			return exp(val2);
		else if (op == "LOG")
			return log(val2);
		else if (op == "SQRT")
			return sqrt(val2);
		else if (op == "CBRT")
			return cbrt(val2);
	}
	else {
		popCount = 2;
		switch (op[0]) {
		case '+':
			return val1 + val2;
		case '-':
			return val1 - val2;
		case '*':
			return val1 * val2;
		case '/':
			return val1 / val2;
		case '^':
			return pow(val1, val2);
		}
	}
	return 0.0;
}