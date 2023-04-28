#include "ast.h"
#include "koopa.h"
#include<iostream>
#include<assert.h>
#include<string.h>
#include <map>
#define IRMAXCHAR 100000
using namespace std;
//extern FILE * ASM;
// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);
void reg_init();
int Release_reg(string reg);

void Sw_stack(string reg, int size);
void Lw_stack(string reg, int size);
void Addi(string reg, int size);
bool Is_stack(const koopa_raw_value_t &value);
int Count_fun_size(const koopa_raw_function_t &func);
int Calc_array_size(const koopa_raw_type_kind *base);

string Alloca_stack(const koopa_raw_value_t &value);


void Visit(const koopa_raw_slice_t &slice);

void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);
void GlobalVisit(const koopa_raw_value_t &value);
void FunVisit(const koopa_raw_value_t &value);
string FunValueVisit(const koopa_raw_value_t &value);

void Visit(const koopa_raw_binary_t &exp);
//void Visit(const koopa_raw_integer_t &ret);
void Visit(const koopa_raw_return_t &ret);
void Visit(const koopa_raw_call_t &value);

void AnalyzeIR(const char *str);

FILE *ASM;

int fun_size = 0;
int ret_fun_size = 0;
int Ra = 0;

const string reg_name[32] = {"x0", "ra", "sp", "gp", "tp",
	"t0", "t1", "t2",
	"fp", "s1",
	"a0", "a1",
	"a2", "a3", "a4", "a5", "a6", "a7",
	"s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
	"t3", "t4", "t5", "t6"};
map<const string, bool> reg_map;
void reg_init()
{
	for (int i = 0; i < 32; i++)
	{ //  |||| ||(i >= 12 && i <= 17)&&
		if ((i >= 6 && i <= 7) || (i >= 28 && i <= 31))
		{
			// cout<<reg_name[i]<<endl;
			reg_map.insert({reg_name[i], false});
		}
	}
}

map<koopa_raw_value_t, int> value_map; //指令存栈

string Alloca_reg()
{
	//  指令分配临时寄存器
	string reg;
	for (auto &i : reg_map)
	{
		if (!i.second)
		{
			i.second = true;
			reg = i.first;
			break;
		}
	}
	return reg;
	//指令结果存在栈中
}

int Release_reg(string reg)
{
	reg_map[reg] = false;
	return 1;
}

void AnalyzeIR(const char *str)
{
	// 解析IR文本str, 得到 Koopa IR 程序
	koopa_program_t program;
	koopa_error_code_t ret = koopa_parse_from_string(str, &program);
	assert(ret == KOOPA_EC_SUCCESS); // 确保解析时没有出错
	// 创建一个 raw program builder, 用来构建 raw program
	koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
	// 将 Koopa IR 程序转换为 raw program
	koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
	// 释放 Koopa IR 程序占用的内存
	koopa_delete_program(program);
	
	// 处理 raw program
	// ...
	reg_init();
	Visit(raw);
	//  处理完成, 释放 raw program builder 占用的内存
	//  注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
	//  所以不要在 raw program 处理完毕之前释放 builder
	koopa_delete_raw_program_builder(builder);
}

void Visit(const koopa_raw_program_t &program)
{
	
	// 执行一些其他的必要操作  // ...
	// 访问所有全局变量
	for (size_t j = 0; j < program.values.len; j++)
	{
		auto value = (koopa_raw_value_t)program.values.buffer[j];
		if (value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
		{
			fprintf(ASM, "  .data\n  .globl %s\n%s:\n", value->name + 1, value->name + 1);
			if (value->kind.data.global_alloc.init->kind.tag == KOOPA_RVT_INTEGER) //非数组
			{
				fprintf(ASM, "  .word %d\n", value->kind.data.global_alloc.init->kind.data.integer.value);
			}
			else
				GlobalVisit(value->kind.data.global_alloc.init);
			fprintf(ASM, "\n");
		}
	}
	
	// 访问所有函数
	for (size_t j = 0; j < program.funcs.len; j++)
	{
		Visit((koopa_raw_function_t)program.funcs.buffer[j]);
	}
}

void GlobalVisit(const koopa_raw_value_t &value)
{
	const auto &kind = value->kind;
	switch (kind.tag)
	{
		// zero
		case KOOPA_RVT_ZERO_INIT: // array
		if (value->ty->tag == KOOPA_RTT_ARRAY && value->ty->data.array.len != 0)
		{
			int sum = value->ty->data.array.len;
			auto temp = value->ty->data.array.base;
			while (temp->tag == KOOPA_RTT_ARRAY)
			{
				sum *= temp->data.array.len;
				temp = temp->data.array.base;
			}
			fprintf(ASM, "  .zero %d\n", sum * 4);
		}
		else if (value->ty->tag == KOOPA_RTT_INT32)
		{
			fprintf(ASM, "  .zero %d\n", 4);
		}
		break;
		/// Aggregate constant.
		case KOOPA_RVT_AGGREGATE: //数组初始化列表 array
		for (size_t i = 0; i < kind.data.aggregate.elems.len; i++)
		{
			koopa_raw_value_t temp = (koopa_raw_value_t)kind.data.aggregate.elems.buffer[i];
			if (temp->kind.tag == KOOPA_RVT_INTEGER)
				fprintf(ASM, "  .word %d\n", temp->kind.data.integer.value);
			else if (temp->kind.tag == KOOPA_RVT_AGGREGATE)
				GlobalVisit(temp);
		}
		break;
		default:
			assert(0);
	}
}

// 访问函数
void Visit(const koopa_raw_function_t &func)
{
	// 执行一些其他的必要操作
	// ... func->name 存的为IR函数全名 比如@main 但riscv只需要main
	// cout<<func->ty->data.function.ret->tag<<endl;
	
	if (func->bbs.len == 0)
	{
		return; //只是函数声明
	}
	else
	{
		Ra = 0;
		fun_size = Count_fun_size(func);
		//对齐16
		ret_fun_size = fun_size;
		fprintf(ASM, "  .text\n");
		fprintf(ASM, "  .globl %s\n%s:\n", func->name + 1, func->name + 1);
		Addi("sp", -fun_size);
		if (Ra)
		{
			fun_size -= 4;
			Sw_stack("ra", fun_size);
		}
		for (size_t j = 0; j < func->bbs.len; ++j)
		{
			koopa_raw_basic_block_t bb = (koopa_raw_basic_block_t)func->bbs.buffer[j];
			fprintf(ASM, ".L%s:\n", bb->name + 1);
			for (size_t k = 0; k < bb->insts.len; ++k)
			{
				koopa_raw_value_t value = (koopa_raw_value_t)bb->insts.buffer[k];
				FunVisit(value);
			}
		}
	}
}

// 访问函数指令
void FunVisit(const koopa_raw_value_t &value)
{
	//指令分配寄存器
	const auto &kind = value->kind;
	//指令分配栈  如果已入栈分配个寄存器使用
	if (Is_stack(value))
	{
		if (value_map.find(value) == value_map.end())
		{
			if (value->kind.tag == KOOPA_RVT_ALLOC)
			{
				if (value->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY)
				{
					auto temp_sum = Calc_array_size(value->ty->data.pointer.base);
					fun_size -= temp_sum * 4;
				}
				else
				{
					fun_size -= 4;
				}
			}
			else
			{
				fun_size -= 4;
			}
			value_map.insert({value, fun_size});
		}
	}
	
	switch (kind.tag)
	{
		/// Binary operation.
	case KOOPA_RVT_BINARY:
		Visit(kind.data.binary);
		break;
		/// Local memory allocation.
		case KOOPA_RVT_ALLOC: //只需要分配栈  *int *int[]
		break;
		/// Memory load.
	case KOOPA_RVT_LOAD:
		{
			string reg = FunValueVisit(kind.data.load.src);
			if (kind.data.load.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR || kind.data.load.src->kind.tag == KOOPA_RVT_GET_PTR)
			{
				fprintf(ASM, "  lw %s, 0(%s)\n", reg.c_str(), reg.c_str());
			}
			Sw_stack(reg, fun_size);
			Release_reg(reg);
		}
		break;
		/// Memory store.
	case KOOPA_RVT_STORE:
		{
			string value_reg = FunValueVisit(kind.data.store.value);
			// string dest_reg=FunValueVisit(kind.data.store.dest);
			//  string dest_reg=Visit(kind.data.store.dest);
			if (kind.data.store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
			{ //如果是全局变量 非数组
				auto reg_dest = Alloca_reg();
				fprintf(ASM, "  la %s, %s\n", reg_dest.c_str(), kind.data.store.dest->name + 1);
				fprintf(ASM, "  sw %s, 0(%s)\n", value_reg.c_str(), reg_dest.c_str());
				Release_reg(reg_dest);
			}
			else if (kind.data.store.dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR || kind.data.store.dest->kind.tag == KOOPA_RVT_GET_PTR)
			{ //数组
				auto reg_dest = FunValueVisit(kind.data.store.dest);
				fprintf(ASM, "  sw %s, 0(%s)\n", value_reg.c_str(), reg_dest.c_str());
				Release_reg(reg_dest);
			}
			else if (kind.data.store.dest->kind.tag == KOOPA_RVT_ALLOC)//局部变量
			{
				Sw_stack(value_reg, value_map[kind.data.store.dest]);
			}
			Release_reg(value_reg);
		}
		break;
		/// Conditional branch.
	case KOOPA_RVT_BRANCH:
		{
			string reg = FunValueVisit(kind.data.branch.cond);
			fprintf(ASM, "  bnez %s, .L%s\n", reg.c_str(), kind.data.branch.true_bb->name + 1);
			fprintf(ASM, "  j .L%s\n", kind.data.branch.false_bb->name + 1);
			Release_reg(reg);
		}
		break;
		/// Unconditional jump.
	case KOOPA_RVT_JUMP:
		fprintf(ASM, "  j .L%s\n", kind.data.jump.target->name + 1);
		break;
		/// Function call.
	case KOOPA_RVT_CALL:
		Visit(kind.data.call);
		if (value->ty->tag == KOOPA_RTT_INT32) // int 必然有返回值
		{
			Sw_stack("a0", fun_size);
		}
		break;
		/// Function return.
	case KOOPA_RVT_RETURN:
		// 访问 return 指令
		Visit(kind.data.ret);
		break;
	case KOOPA_RVT_GET_PTR:
		{
			string src_reg = FunValueVisit(kind.data.get_ptr.src);
			string index_reg = FunValueVisit(kind.data.get_ptr.index); //一种数表达式，一种是int
			
			if (kind.data.get_ptr.src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY)
			{
				int temp_sum = Calc_array_size(kind.data.get_elem_ptr.src->ty->data.pointer.base);
				fprintf(ASM, "  li t0, %d\n", temp_sum * 4);
				fprintf(ASM, " mul %s, %s, t0\n", index_reg.c_str(), index_reg.c_str());
			}
			else
			{
				fprintf(ASM, "  slli %s, %s, 2\n", index_reg.c_str(), index_reg.c_str());
			}
			fprintf(ASM, "  add  %s, %s, %s\n", src_reg.c_str(), src_reg.c_str(), index_reg.c_str());
			Sw_stack(src_reg, fun_size);
			Release_reg(src_reg);
			Release_reg(index_reg);
		}
		break;
		/// Element pointer calculation.
	case KOOPA_RVT_GET_ELEM_PTR:
		{
			string src_reg = FunValueVisit(kind.data.get_elem_ptr.src);
			string index_reg = FunValueVisit(kind.data.get_elem_ptr.index); //一种数表达式，一种是int
			//要计算除了最高层的维数(kind.data.get_elem_ptr.src->ty->data.array.len)
			if (kind.data.get_elem_ptr.src->ty->data.pointer.base->data.array.base->tag == KOOPA_RTT_ARRAY)
			{
				int temp_sum = Calc_array_size(kind.data.get_elem_ptr.src->ty->data.pointer.base->data.array.base);
				fprintf(ASM, "  li t0, %d\n", temp_sum * 4);
				fprintf(ASM, " mul %s, %s, t0\n", index_reg.c_str(), index_reg.c_str());
			}
			else
			{
				fprintf(ASM, "  slli %s, %s, 2\n", index_reg.c_str(), index_reg.c_str());
			}
			fprintf(ASM, "  add  %s, %s, %s\n", src_reg.c_str(), src_reg.c_str(), index_reg.c_str());
			Sw_stack(src_reg, fun_size);
			Release_reg(src_reg);
			Release_reg(index_reg);
		}
		break;
		default:
			// 其他类型暂时遇不到
			assert(false);
	}
	fprintf(ASM, "\n");
}

string FunValueVisit(const koopa_raw_value_t &value)
{
	string reg;
	reg = Alloca_stack(value);
	if (!reg.empty())
		return reg;
	
	const auto &kind = value->kind;
	switch (kind.tag)
	{
	case KOOPA_RVT_INTEGER:
		reg = Alloca_reg();
		fprintf(ASM, "  li %s,%d\n", reg.c_str(), kind.data.integer.value);
		break;
		/// Function argument reference.
		case KOOPA_RVT_FUNC_ARG_REF: // int *int
		//函数参数,reg会在其参与指令释放
		reg = Alloca_reg();
		if (kind.data.func_arg_ref.index < 8)
			fprintf(ASM, "  mv %s,a%zu\n", reg.c_str(), kind.data.func_arg_ref.index);
		else
			fprintf(ASM, "  lw %s,%lu(sp)\n", reg.c_str(), (kind.data.func_arg_ref.index - 8) * 4 + ret_fun_size);
		break;
		case KOOPA_RVT_GLOBAL_ALLOC: //不包含store指令的dest 引用全局变量 处理类似常量   *int *int[]
		reg = Alloca_reg();
		fprintf(ASM, "  la %s,%s\n", reg.c_str(), value->name + 1);
		if (value->ty->tag == KOOPA_RTT_POINTER)
		{ //如果指向数组指针
			if (value->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY)
			{
			}
			else if (value->ty->data.pointer.base->tag == KOOPA_RTT_INT32)
			{
				fprintf(ASM, "  lw %s,0(%s)\n", reg.c_str(), reg.c_str());
			}
		}
		break;
		default:
			break;
	}
	return reg;
}
//二元运算
void Visit(const koopa_raw_binary_t &exp)
{
	string lhs_reg = FunValueVisit(exp.lhs);
	string rhs_reg = FunValueVisit(exp.rhs);
	string reg = lhs_reg;
	switch (exp.op)
	{
	case KOOPA_RBO_NOT_EQ:
		// fprintf(ASM," xor %s, %s ,%s")
		fprintf(ASM, "  xor %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		fprintf(ASM, "  snez %s, %s\n", reg.c_str(), reg.c_str());
		break;
		/// Equal to.
	case KOOPA_RBO_EQ:
		fprintf(ASM, "  xor %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		fprintf(ASM, "  seqz %s, %s\n", reg.c_str(), reg.c_str());
		break;
		/// Greater than.
	case KOOPA_RBO_GT:
		fprintf(ASM, "  sgt %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Less than.
	case KOOPA_RBO_LT:
		fprintf(ASM, "  slt %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Greater than or equal to.
	case KOOPA_RBO_GE:
		fprintf(ASM, "  sub %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		fprintf(ASM, " li  t0, -1\n");
		fprintf(ASM, " sgt %s, %s, t0\n", reg.c_str(), reg.c_str());
		break;
		/// Less than or equal to.
	case KOOPA_RBO_LE:
		fprintf(ASM, "  sub %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		fprintf(ASM, " li  t0, 1\n");
		fprintf(ASM, " slt %s, %s, t0\n", reg.c_str(), reg.c_str());
		break;
		/// Addition.
	case KOOPA_RBO_ADD:
		fprintf(ASM, "  add %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Subtraction.
	case KOOPA_RBO_SUB:
		fprintf(ASM, "  sub %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Multiplication.
	case KOOPA_RBO_MUL:
		fprintf(ASM, "  mul %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Division.
	case KOOPA_RBO_DIV:
		fprintf(ASM, "  div %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Modulo.
	case KOOPA_RBO_MOD:
		fprintf(ASM, "  rem %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Bitwise AND.
	case KOOPA_RBO_AND:
		fprintf(ASM, "  and %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Bitwise OR.
	case KOOPA_RBO_OR:
		fprintf(ASM, "  or %s, %s, %s\n", reg.c_str(), lhs_reg.c_str(), rhs_reg.c_str());
		break;
		/// Bitwise XOR.
	case KOOPA_RBO_XOR:
		break;
		default:
			break;
	}
	Sw_stack(reg, fun_size);
	Release_reg(lhs_reg);
	Release_reg(rhs_reg);
}

// call fun指令
void Visit(const koopa_raw_call_t &fun)
{
	for (size_t i = 0; i < fun.args.len; i++)
	{
		string reg = FunValueVisit((koopa_raw_value_t)fun.args.buffer[i]);
		if (i < 8)
		{
			fprintf(ASM, "  mv a%zu,%s\n", i, reg.c_str());
		}
		else
		{
			Sw_stack(reg, (int(i) - 8) * 4);
		}
		Release_reg(reg);
	}
	fprintf(ASM, "  call %s\n", fun.callee->name + 1);
}

// return 指令
void Visit(const koopa_raw_return_t &ret)
{
	string reg = "a0";
	if (ret.value)
	{
		string ret_reg = FunValueVisit(ret.value);
		fprintf(ASM, "  move %s,%s\n", reg.c_str(), ret_reg.c_str());
		Release_reg(ret_reg);
	}
	if (Ra)
	{
		Lw_stack("ra", ret_fun_size - 4);
	}
	Addi("sp", ret_fun_size);
	fprintf(ASM, "ret\n\n\n");
}

//函数栈空间大小
int Count_fun_size(const koopa_raw_function_t &func)
{
	int sum = 0;
	int max_len = 0;
	for (size_t j = 0; j < func->bbs.len; ++j)
	{
		koopa_raw_basic_block_t bb = (koopa_raw_basic_block_t)func->bbs.buffer[j]; // 进一步处理当前基本块
		for (size_t k = 0; k < bb->insts.len; ++k)
		{
			koopa_raw_value_t value = (koopa_raw_value_t)bb->insts.buffer[k];
			if (value->ty->tag != KOOPA_RTT_UNIT)
			{
				if (value->kind.tag == KOOPA_RVT_ALLOC)
				{
					if (value->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY)
					{
						int temp_sum =Calc_array_size(value->ty->data.pointer.base);
						sum += temp_sum * 4;
					}
					else
					{
						sum += 4;
					}
				}
				else
				{
					sum += 4;
				}
			}
			if (value->kind.tag == KOOPA_RVT_CALL)
			{
				Ra = 4;
				if (max_len < int(value->kind.data.call.args.len) - 8)
					max_len = value->kind.data.call.args.len - 8;
				// value->kind.data.
			}
		}
	}
	sum += max_len * 4;
	sum = sum % 16 ? (sum / 16) * 16 + 16 : sum;
	return sum;
}

//写入栈
void Sw_stack(string reg, int size)
{
	if (size >= -2048 && size <= 2047)
	{
		fprintf(ASM, "  sw %s, %d(sp)\n", reg.c_str(), size);
	}
	else
	{
		fprintf(ASM, "  li t0, %d\n", size);
		fprintf(ASM, "  add t0, sp, t0\n");
		fprintf(ASM, "  sw %s, 0(t0)\n", reg.c_str());
	}
}

//从栈写入寄存器
void Lw_stack(string reg, int size)
{
	if (size >= -2048 && size <= 2047)
	{
		fprintf(ASM, "  lw %s, %d(sp)\n", reg.c_str(), size);
	}
	else
	{
		fprintf(ASM, "  li t0, %d\n", size);
		fprintf(ASM, "  add t0, sp, t0\n");
		fprintf(ASM, "  lw %s, 0(t0)\n", reg.c_str());
	}
}

//
void Addi(string reg, int size)
{
	if (size >= -2048 && size <= 2047)
	{
		fprintf(ASM, "  addi %s, sp, %d\n", reg.c_str(), size);
	}
	else
	{
		fprintf(ASM, "  li t0, %d\n", size);
		fprintf(ASM, "  add %s, sp, t0\n", reg.c_str());
	}
}

//分配栈
bool Is_stack(const koopa_raw_value_t &value)
{
	auto tag = value->kind.tag;
	if (tag == KOOPA_RVT_BINARY) // int
	{
		return true;
	}
	else if (tag == KOOPA_RVT_ALLOC) // point
	{
		return true;
	}
	else if (tag == KOOPA_RVT_LOAD) // point int
	{
		return true;
	}
	else if (tag == KOOPA_RVT_GET_PTR) // point
	{
		return true;
	}
	else if (tag == KOOPA_RVT_GET_ELEM_PTR) // point
	{
		return true;
	}
	else if (tag == KOOPA_RVT_CALL) // int void
	{
		if (value->ty->tag == KOOPA_RTT_INT32)
		{
			return true;
		}
	}
	return false;
}

string Alloca_stack(const koopa_raw_value_t &value)
{
	string reg;
	if (value_map.find(value) != value_map.end())
	{ //已入栈分配函数
		reg = Alloca_reg();
		if (value->kind.tag == KOOPA_RVT_ALLOC)
		{                                                           //如果指向数组指针
			if (value->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY) // alloc array
			{
				Addi(reg, value_map[value]);
			}
			else if (value->ty->data.pointer.base->tag == KOOPA_RTT_INT32) // alloc int
			{
				Lw_stack(reg, value_map[value]);
			}
			else if(value->ty->data.pointer.base->tag==KOOPA_RTT_POINTER){
				Lw_stack(reg, value_map[value]);
			}
		}
		else if (value->ty->tag != KOOPA_RTT_UNIT) //二元运算 load *int
		{
			Lw_stack(reg, value_map[value]);
		}
	}
	return reg;
}

int Calc_array_size(const koopa_raw_type_kind * base)
{
	auto temp = base->data.array.base;
	int sum=base->data.array.len;
	while (temp->tag == KOOPA_RTT_ARRAY)
	{
		sum *= int(temp->data.array.len);
		temp = temp->data.array.base;
	}
	return sum;
}

//ASTDUMP

int BaseAST::Count_Order = -1;
string btype_str;
FILE *IR;

map<string, Symbol> glo_symbolmap;
map<string, int> all_fun_symtab;
bool global = false;

Fun_sym fun_symtab;
map<string, Symbol> symbolmap;

int ret_flag =0;
int IF_cnt = 0;
int While_cnt = 0;
vector<int> vec_while;
int Break_cnt = 0;
int Continue_cnt = 0;
int result_cnt=0;
int fun_num=-1;
void Fun_init()
{
	fun_num++;
	//ret_flag = 0;
	//IF_cnt = 0;
	//While_cnt = 0;
	//vec_while.clear();
	//Break_cnt = 0;
	//Continue_cnt = 0;
	//result_cnt=0;
	BaseAST::Count_Order = -1;
	fun_symtab.clear();
	symbolmap.clear();
}

vector<int> vec_array_constexp; //储存数组各个维数len

//数组初始化
vector<int> vec_block_len;      //各层{}初始化单位 类似符号表的思想
vector<string> vec_initval;     //补全的初始化列表
int exp_cnt = 0;                // exp_cnt在使用过后以及{}结束后都要重置
int arrayout = 0;               //全局初始化列表使用
vector<string> vec_point_array; //局部数组指针赋值用
//数组访问
vector<string> vec_array_exp;
int vec_array_exp_len;

void Array_init()
{
	exp_cnt = 0;
	arrayout = 0;
	vec_point_array.clear();
	
	vec_block_len.clear();
	vec_array_constexp.clear(); //数组维度
	vec_initval.clear();        //数组初始值
	
	vec_array_exp.clear();
}

void Count_block_len()
{
	if (vec_block_len.size() == 0) //初始化
	{
		int temp = 1;
		for (auto i : vec_array_constexp)
		{
			temp *= i;
		}
		vec_block_len.push_back(temp);
		return;
	}
	if (exp_cnt != 0)
	{
		int temp = 1;
		for (int i = vec_array_constexp.size() - 1; i >= 0; i--)
		{
			exp_cnt /= vec_array_constexp[i];
			if (exp_cnt)
			{
				temp *= vec_array_constexp[i];
			}
			else
				break;
		}
		vec_block_len.push_back(temp);
	}
	else if (exp_cnt == 0)
	{
		int temp = vec_block_len.back();
		int block_len = 1;
		for (int i = vec_array_constexp.size() - 1; i >= 0; i--)
		{
			temp /= vec_array_constexp[i];
			if (temp == 1)
			{
				break;
			}
			else
			{
				block_len *= vec_array_constexp[i];
			}
		}
		vec_block_len.push_back(block_len);
	}
	exp_cnt = 0;
}

void DumpGlobalArray(int depth) //全局数组初始化列表补全输出
{
	if (depth < vec_array_constexp.size() - 1)
	{
		for (int i = 0; i < vec_array_constexp[depth]; i++)
		{
			fprintf(IR, "{ ");
			DumpGlobalArray(depth + 1);
			fprintf(IR, " }");
			if (i != vec_array_constexp[depth] - 1)
				fprintf(IR, " , ");
		}
	}
	else
	{
		for (int i = 0; i < vec_array_constexp[depth]; i++)
		{
			fprintf(IR, "%s", vec_initval[arrayout++].c_str());
			if (i != vec_array_constexp[depth] - 1)
				fprintf(IR, ",");
		}
	}
}

void DumpFuncArray(int depth, string array_name) //输出局部初始化数组 包括const和非const
{
	if (depth == 0)
		vec_point_array.push_back("@" + array_name);
	
	for (int i = 0; i < vec_array_constexp[depth]; i++)
	{
		BaseAST::Count_Order++;
		string temp = string("%" + to_string(BaseAST::Count_Order));
		fprintf(IR, "  %s = getelemptr %s, %d\n", temp.c_str(), vec_point_array.back().c_str(), i);
		vec_point_array.push_back(temp);
		if (depth == vec_array_constexp.size() - 1)
		{
			fprintf(IR, "  store %s, %s\n", vec_initval[arrayout++].c_str(), temp.c_str());
		}
		else
			DumpFuncArray(depth + 1, array_name);
		vec_point_array.pop_back();
	}
}

string Visit_array(string array_num, int block_num, string type)
{
	// if(vec_array_exp.empty())
	string temp;
	if (block_num != -1)
	{
		array_num = "@" + array_num + "_" + to_string(block_num);
	}
	else
		array_num = "@" + array_num;
	
	vector<string>  vec_array_exp_temp;
	for(int i=0;i<vec_array_exp_len;i++){
		vec_array_exp_temp.push_back( vec_array_exp.back() );
		vec_array_exp.pop_back();
	}
	if (type[0] == '*')
	{
		temp = "%" + to_string(++BaseAST::Count_Order);
		fprintf(IR, "  %s = load %s\n", temp.c_str(), array_num.c_str());
		array_num = temp;
		if (vec_array_exp_len != 0)
		{
			temp = "%" + to_string(++BaseAST::Count_Order);
			fprintf(IR, "  %s = getptr %s, %s\n", temp.c_str(), array_num.c_str(), vec_array_exp_temp.back().c_str());
			vec_array_exp_temp.pop_back();
			array_num = temp;
		}
	}
	while(!vec_array_exp_temp.empty())
	{
		temp = "%" + to_string(++BaseAST::Count_Order);
		fprintf(IR, "  %s = getelemptr %s, %s\n", temp.c_str(), array_num.c_str(), vec_array_exp_temp.back().c_str());
		vec_array_exp_temp.pop_back();
		array_num = temp;
	}
	return temp;
}

Symbol Symbol_find(string str)
{
	Symbol node;
	if (symbolmap.find(str) != symbolmap.end())
	{
		return symbolmap[str];
	}
	int i = fun_symtab.vec_symbolmap.size() - 1;
	for (; i >= 0; i--)
	{
		if (fun_symtab.vec_symbolmap[i].find(str) != fun_symtab.vec_symbolmap[i].end())
		{
			node = fun_symtab.vec_symbolmap[i][str];
			return node;
		}
	}
	if (glo_symbolmap.find(str) != glo_symbolmap.end())
	{
		return glo_symbolmap[str];
	}
	assert(i != -1);
	return node;
}

string Dumpop(string temp1, string temp2, string op)
{
	//目前看op1 op2 都不需要
	
	char temp[MAXCHARS] = {0};
	BaseAST::Count_Order++;
	sprintf(temp, "%%%d", BaseAST::Count_Order);
	fprintf(IR, "  %s = %s %s, %s\n", temp, op.c_str(), temp1.c_str(), temp2.c_str());
	return temp;
}

string DumpUnaryOp(string temp1, string op)
{
	char temp[MAXCHARS] = {0};
	BaseAST::Count_Order++;
	sprintf(temp, "%%%d", BaseAST::Count_Order);
	fprintf(IR, "  %s = %s 0, %s\n", temp, op.c_str(), temp1.c_str());
	return temp;
}

string DumpLoad(string lval, int block_num)
{
	char temp[MAXCHARS] = {0};
	BaseAST::Count_Order++;
	sprintf(temp, "%%%d", BaseAST::Count_Order);
	fprintf(IR, "  %s = load @%s", temp, lval.c_str());
	if (block_num != -1)
	{ //-1为全局量
		fprintf(IR, "_%d", block_num);
	}
	fprintf(IR, "\n");
	return temp;
}

string DumpStore(string temp1, string lval, int block_num)
{
	fprintf(IR, "  store %s, @%s", temp1.c_str(), lval.c_str());
	if (block_num != -1)
	{ //-1为全局量
		fprintf(IR, "_%d", block_num);
	}
	fprintf(IR, "\n\n");
	return temp1;
}

string DumpAlloc(string temp1, int block_num, string type)
{
	fprintf(IR, "  @%s", temp1.c_str());
	if (block_num != -1)
	{
		fprintf(IR, "_%d", block_num);
	}
	fprintf(IR, "= alloc %s\n", type.c_str());
	return temp1;
}

string DumpIfElse(unique_ptr<BaseAST> &exp, unique_ptr<BaseAST> &then_block, unique_ptr<BaseAST> &else_block, int con_num)
{
	string temp = exp->Dump();
	fprintf(IR, "  br %s, %%then_%d, %%else_%d\n\n", temp.c_str(), con_num, con_num);
	
	fprintf(IR, "%%then_%d:\n", con_num);
	then_block->Dump();
	fprintf(IR, "  jump %%if_end_%d\n\n", con_num);
	
	fprintf(IR, "%%else_%d:\n", con_num);
	else_block->Dump();
	fprintf(IR, "  jump %%if_end_%d\n\n", con_num);
	
	fprintf(IR, "%%if_end_%d:\n", con_num);
	return temp;
}
string DumpIf(unique_ptr<BaseAST> &exp, unique_ptr<BaseAST> &then_block, int con_num)
{
	string temp = exp->Dump();
	fprintf(IR, "  br %s, %%then_%d, %%if_end_%d\n\n", temp.c_str(), con_num, con_num);
	fprintf(IR, "%%then_%d:\n", con_num);
	then_block->Dump();
	fprintf(IR, "  jump %%if_end_%d\n\n", con_num);
	
	fprintf(IR, "%%if_end_%d:\n", con_num);
	return temp;
}

string DumpWhile(unique_ptr<BaseAST> &exp, unique_ptr<BaseAST> &body, int con_num)
{
	vec_while.push_back(con_num);
	string temp;
	fprintf(IR, "  jump %%while_entry_%d\n\n", con_num);
	
	fprintf(IR, "%%while_entry_%d:\n", con_num);
	temp = exp->Dump();
	fprintf(IR, "  br %s, %%while_body_%d, %%while_end_%d\n\n", temp.c_str(), con_num, con_num);
	
	fprintf(IR, "%%while_body_%d:\n", con_num);
	body->Dump();
	fprintf(IR, "  jump %%while_entry_%d\n\n", con_num);
	
	fprintf(IR, "%%while_end_%d:\n", con_num);
	vec_while.pop_back();
	return temp;
}

string DumpCall(string ident, string param)
{
	char temp[MAXCHARS] = {0};
	if (all_fun_symtab[ident] == 1)
	{ // void;
		fprintf(IR, "  call @%s(%s)\n", ident.c_str(), param.c_str());
	}
	else if (all_fun_symtab[ident] == 2)
	{
		BaseAST::Count_Order++;
		sprintf(temp, "%%%d", BaseAST::Count_Order);
		fprintf(IR, "  %s = call @%s(%s)\n", temp, ident.c_str(), param.c_str());
	}
	return temp;
}

void Decl()  //一些sys自带库函数
{
	fprintf(IR, "decl @getint(): i32\ndecl @getch(): i32\ndecl @getarray(*i32): i32\ndecl @putint(i32)\ndecl @putch(i32)\ndecl @putarray(i32, *i32)\ndecl @starttime()\ndecl @stoptime()\n\n");
	all_fun_symtab.insert({"getint", 2});
	all_fun_symtab.insert({"getch", 2});
	all_fun_symtab.insert({"getarray", 2});
	all_fun_symtab.insert({"putint", 1});
	all_fun_symtab.insert({"putch", 1});
	all_fun_symtab.insert({"putarray", 1});
	all_fun_symtab.insert({"starttime", 1});
	all_fun_symtab.insert({"stoptime", 1});
}


int main(int argc, const char *argv[])
{
  // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
  // compiler 模式 输入文件 -o 输出文件
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
  yyin = fopen(input, "r");
  assert(yyin);

  // 调用 parser 函数, parser 函数会进一步调用 lexer 解析输入文件的
  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);
  //
  if (strcmp("-koopa", mode) == 0)
  {
    IR=fopen(output,"w");
    assert(IR);
    Decl();
    ast->Dump();
    fclose(IR);
    
    #ifdef DEBUG
    IR=fopen(output,"r");
    char str[MAXCHARS]={0};
    size_t len=fread(str,sizeof(char),MAXCHARS,IR);
    str[len]='\0';
    cout<<str;
    fclose(IR);
    #endif

  }
  else if(strcmp("-riscv",mode)==0){
    
    IR=fopen("out.koopa", "w");
    assert(IR);
    Decl(); 
    ast->Dump();
    fclose(IR);

    IR= fopen("out.koopa","r+");
    assert(IR);
    char  str[MAXCHARS]={0};
    size_t len=fread(str,sizeof(char),MAXCHARS,IR);
    str[len]='\0';
    fclose(IR);
   
    ASM=fopen(output,"w");
    assert(ASM);
    AnalyzeIR(str);
    fclose(ASM);

    #ifdef DEBUG
    ASM=fopen(output,"r");
    memset(str,0,sizeof(str));
    len=fread(str,sizeof(char),MAXCHARS,ASM);
    str[len]='\0';
    cout<<str;
    fclose(ASM);
    #endif

  }
  return 0;
}
