#pragma once
#ifndef GRANDPARENT_H
#define GRANDPARENT_H

#define DEBUG
#define MAXCHARS 1000000


#include <cassert>
#include <iostream>
#include <cstdio>
#include <memory>
#include <cstring>
#include <map>
#include <vector>

using namespace std;

class BaseAST
{
public:
	virtual ~BaseAST() = default;
	static int Count_Order;
	// int count; //代表该节点结果的代数
	int kind; //字节点序号
	
	virtual string Dump() = 0;
	virtual int Calc() = 0;
};

extern string btype_str; // 声明时变量类型，以便所有声明的变量种类初始化
extern FILE *IR;

extern int ret_flag ;
extern int While_cnt; //记录条件指令个数
extern vector<int> vec_while;
extern int IF_cnt;
extern int Break_cnt;
extern int Continue_cnt;
extern int result_cnt;
extern int fun_num;

void Array_init();
extern vector<int> vec_array_constexp;
extern vector<string> vec_initval;
extern vector<int> vec_block_len;
extern int exp_cnt;
void Count_block_len();
void DumpGlobalArray(int depth);
void DumpFuncArray(int depth, string array_name);
extern vector<string> vec_array_exp;
extern int vec_array_exp_len;

string Visit_array(string array_num, int block_num, string type);

void Fun_init();

struct Symbol
{
	int kind;      // 0为局部const常量 1为局部符号变量 2为全局const 3为全局变量
	int val;       // 没有初始化时，默认为0, 数组用val记录维数
	string type;   //变量类型i32  [[i32,len_max],len_max-1] *
	int block_num; //记录符号所属于的block
};

struct Fun_sym //每个函数符号表
{
	vector<map<string, Symbol>> vec_symbolmap; //函数内每个块的符号表
	int block_num;                             //记录当前block的序号
	int block_cnt;                             // 记录所有block的块的个数，用于命名防止名称重复
	int type;                                  //函数类型1为void  2为int
	Fun_sym()
	{
		block_num = 0;
		block_cnt = 0;
		type = 0;
	}
	void clear()
	{
		block_num = 0;
		block_cnt = 0;
		type = 0;
		vec_symbolmap.clear();
	}
};
extern map<string, int> all_fun_symtab;   //所有函数的种类
extern map<string, Symbol> glo_symbolmap; //全局变量
extern bool global;

extern Fun_sym fun_symtab;            //当前函数的符号表
extern map<string, Symbol> symbolmap; //代表当前block符号表,便于操作

Symbol Symbol_find(string str);



void Decl();
string Dumpop(string temp1, string temp2, string op);
string DumpUnaryOp(string temp1, string op);
string DumpLoad(string lval, int block_num);
string DumpStore(string temp1, string lval, int block_num);
string DumpAlloc(string temp1, int block_num, string type);
string DumpWhile(unique_ptr<BaseAST> &exp, unique_ptr<BaseAST> &body, int con_num);
string DumpIfElse(unique_ptr<BaseAST> &exp, unique_ptr<BaseAST> &then_block, unique_ptr<BaseAST> &else_block, int con_num);
string DumpIf(unique_ptr<BaseAST> &exp, unique_ptr<BaseAST> &then_block, int con_num);
string DumpCall(string ident, string param);

class AST : public BaseAST
{
public:
	unique_ptr<BaseAST> compunit;
	string Dump() override
	{
		return compunit->Dump();
	}
	int Calc() override
	{
		return 0;
	}
};

////CompUnit:Unit|CompUnit Unit;
class CompUnitast : public BaseAST
{
public:
	unique_ptr<BaseAST> compunit;
	unique_ptr<BaseAST> unit;
	string Dump() override // override确保虚函数覆盖基类的虚函数
	{
		string temp;
		if (kind == 1)
		{
			unit->Dump();
		}
		else if (kind == 2)
		{
			compunit->Dump();
			unit->Dump();
		}
		return temp;
	}
	
	int Calc() override
	{
		return 0;
	}
};

// Unit :FuncDef|VarDecl|ConstDecl;  //全局变量
class Unitast : public BaseAST
{
public:
	unique_ptr<BaseAST> funcdef;
	unique_ptr<BaseAST> vardecl;
	unique_ptr<BaseAST> constdecl;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			// all_symtab.vec_fun_symtab
			funcdef->Dump();
		}
		else if (kind == 2)
		{
			global = true;
			vardecl->Dump();
			global = false;
		}
		else if (kind == 3)
		{
			global = true;
			constdecl->Calc();
			global = false;
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// BType         ::= "int";
class BTypeast : public BaseAST
{
public:
	string str;
	string Dump() override
	{
		return str;
	}
	int Calc() override
	{
		return 0;
	}
};

// ValType : |ArrayDef
class ValTypeast : public BaseAST
{
public:
	unique_ptr<BaseAST> arraydef;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = "i32";
		}
		else if (kind == 2)
		{
			temp = arraydef->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};
// ArrayDef : '[' ConstExp ']'|'[' ConstExp ']' ArrayDef
class ArrayDefast : public BaseAST
{ //记录下数组维数
public:
	unique_ptr<BaseAST> constexp;
	unique_ptr<BaseAST> arraydef;
	string Dump() override
	{ //返回值为数组类型string 并在vec_array_constexp中记录各个维数
		string temp;
		if (kind == 1)
		{
			int val = constexp->Calc();
			vec_array_constexp.push_back(val);
			temp = "[i32," + to_string(val) + "]";
		}
		else if (kind == 2)
		{
			int val = constexp->Calc();
			vec_array_constexp.push_back(val);
			temp = "[" + arraydef->Dump() + "," + to_string(val) + "]";
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// Decl          ::= ConstDecl|VarDecl;
class Declast : public BaseAST
{
public:
	unique_ptr<BaseAST> constdecl;
	unique_ptr<BaseAST> vardecl;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			constdecl->Calc();
		}
		else if (kind == 2)
		{
			temp = vardecl->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};
// ConstDecl     ::= "const" BType ConstDef ";";
class ConstDeclast : public BaseAST
{
public:
	unique_ptr<BaseAST> btype; // btype 目前都为int 先不管
	unique_ptr<BaseAST> constdef;
	string Dump() override
	{
		return "";
	}
	int Calc() override
	{
		btype_str = btype->Dump();
		return constdef->Calc();
	}
};

// ConstDef      ::= IDENT ValType "=" ConstInitVal|IDENT ValType "=" ConstInitVal ',' ConstDef;
class ConstDefast : public BaseAST
{
public:
	unique_ptr<BaseAST> constinitval;
	string ident;
	unique_ptr<BaseAST> valtype;
	unique_ptr<BaseAST> constdef;
	string Dump() override
	{
		return "";
	}
	int Calc() override
	{
		
		Array_init();
		string type = valtype->Dump();
		
		if (type == "i32") //不是数组
		{
			int val = constinitval->Calc();
			if (!global)
			{
				symbolmap.insert({ident, {0, val, btype_str, fun_symtab.block_num}});
			}
			else
			{
				glo_symbolmap.insert({ident, {2, val, btype_str, -1}});
			}
#ifdef DEBUG
			cout << "const:" << ident << " " << val << endl;
#endif
		}
		else
		{ // const 数组需要存到IR里面
			constinitval->Dump();
			if (global)
			{
				fprintf(IR, "global @%s = alloc %s , {", ident.c_str(), type.c_str());
				DumpGlobalArray(0);
				fprintf(IR, " }\n");
				glo_symbolmap.insert({ident, {2, int(vec_array_constexp.size()), type, -1}});
			}
			else
			{
				DumpAlloc(ident, fun_symtab.block_num, type);
				DumpFuncArray(0, ident + "_" + to_string(fun_symtab.block_num));
				fprintf(IR, "\n");
				symbolmap.insert({ident, {0, int(vec_array_constexp.size()), type, fun_symtab.block_num}});
			}
		}
		
		if (kind == 2)
		{
			constdef->Calc();
		}
		return 0;
	}
};

// ConstInitVal :ConstExp|'{' '}' |'{'  ConstArrayVal '}';
class ConstInitValast : public BaseAST
{
public:
	unique_ptr<BaseAST> constexp;
	unique_ptr<BaseAST> constarrayval;
	// int size; //当前的初始化单位的
	string Dump() override
	{
		string temp;
		
		if (kind == 1)
		{
			exp_cnt++;
			vec_initval.push_back(to_string(constexp->Calc()));
		}
		else if (kind == 2)
		{
			
			Count_block_len();
			int size = vec_block_len.back();
			for (int i = 0; i < size; i++)
			{
				vec_initval.push_back("0");
			}
			vec_block_len.pop_back();
			exp_cnt = 0;
		}
		else if (kind == 3)
		{
			Count_block_len();
			int vec_add = vec_initval.size();
			temp = constarrayval->Dump();
			vec_add = vec_initval.size() - vec_add;
			
			int size = vec_block_len.back();
			for (; vec_add < size; vec_add++)
			{
				vec_initval.push_back("0");
			}
			vec_block_len.pop_back();
			exp_cnt = 0;
		}
		return temp;
	}
	int Calc() override
	{
		return constexp->Calc();
	}
};
// ConstArrayVal: ConstInitVal | ConstInitval ',' ConstArrayVal;
class ConstArrayValast : public BaseAST //避免最外层括号的判断直接从constarrayval开始递归
{
public:
	unique_ptr<BaseAST> constinitval;
	unique_ptr<BaseAST> constarrayval;
	// int depth;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			constinitval->Dump();
		}
		else if (kind == 2)
		{
			constinitval->Dump();
			constarrayval->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// ConstExp      ::= Exp;
class ConstExpast : public BaseAST
{
public:
	unique_ptr<BaseAST> exp;
	string Dump() override
	{
		return "";
	}
	int Calc() override
	{
		return exp->Calc();
	}
};

// VarDecl       ::= BType VarDef  ";";
class VarDeclast : public BaseAST
{
public:
	unique_ptr<BaseAST> btype;
	unique_ptr<BaseAST> vardef;
	string Dump() override
	{ // alloc
		btype_str = btype->Dump();
		return vardef->Dump();
	}
	int Calc() override
	{
		return 0;
	}
};

// VarDef: IDENT ValType| IDENT ValType "=" InitVal| IDENT ValType ',' VarDef|IDENT ValType "=" InitVal ',' VarDef;
class VarDefast : public BaseAST
{
public:
	string ident;
	unique_ptr<BaseAST> initval;
	unique_ptr<BaseAST> vardef;
	unique_ptr<BaseAST> valtype;
	string Dump() override
	{
		string temp;
		
		Array_init();
		string type = valtype->Dump();
		if (global)
		{ //和const array相同
			int val = vec_array_constexp.size();
			fprintf(IR, "global @%s = alloc %s, ", ident.c_str(), type.c_str());
			if (kind == 2 || kind == 4)
			{
				
				if (type == "i32")
				{
					val = initval->Calc();
					fprintf(IR, "%d\n", val);
				}
				else
				{
					initval->Calc();
					fprintf(IR, "{ ");
					DumpGlobalArray(0);
					fprintf(IR, " }\n");
				}
			}
			else
			{
				fprintf(IR, "zeroinit\n");
			}
			glo_symbolmap.insert({ident, {3, val, type, -1}});
		}
		else if (!global)
		{
			DumpAlloc(ident, fun_symtab.block_num, type);
			if (kind == 2 || kind == 4)
			{
				if (type == "i32")
				{
					DumpStore(initval->Dump(), ident, fun_symtab.block_num);
				}
				else
				{
					initval->Calc();
					DumpFuncArray(0, ident + "_" + to_string(fun_symtab.block_num));
					fprintf(IR, "\n");
				}
			}
			else
			{
				fprintf(IR, "\n");
			}
			symbolmap.insert({ident, {1, int(vec_array_constexp.size()), type, fun_symtab.block_num}});
		}
		if (kind == 3 || kind == 4)
		{
			vardef->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// InitVal   :Exp | '{' '}'|'{' ArrayInitVal '}';
class InitValast : public BaseAST
{
public:
	unique_ptr<BaseAST> exp;
	unique_ptr<BaseAST> arrayinitval;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = exp->Dump();
		}
		else if (kind == 2)
		{
		}
		else if (kind == 3)
		{
		}
		return temp;
	}
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			exp_cnt++;
			if (global)
			{
				temp = exp->Calc();
				vec_initval.push_back(to_string(temp));
			}
			else
			{
				vec_initval.push_back(exp->Dump());
			}
		}
		else if (kind == 2)
		{
			Count_block_len();
			int size = vec_block_len.back();
			for (int i = 0; i < size; i++)
			{
				vec_initval.push_back("0");
			}
			vec_block_len.pop_back();
			exp_cnt = 0;
		}
		else if (kind == 3)
		{
			Count_block_len();
			int vec_add = vec_initval.size();
			arrayinitval->Calc();
			vec_add = vec_initval.size() - vec_add;
			
			int size = vec_block_len.back();
			for (; vec_add < size; vec_add++)
			{
				vec_initval.push_back("0");
			}
			vec_block_len.pop_back();
			exp_cnt = 0;
		}
		return temp;
	}
};
// ArrayInitVal  : InitVal| InitVal ',' ArrayInitVal;
class ArrayInitValast : public BaseAST
{
public:
	unique_ptr<BaseAST> initval;
	unique_ptr<BaseAST> arrayinitval;
	string Dump() override
	{
		string temp;
		return temp;
	}
	int Calc() override
	{
		if (kind == 1)
		{
			initval->Calc();
		}
		else if (kind == 2)
		{
			initval->Calc();
			arrayinitval->Calc();
		}
		return 0;
	}
};

// Block         ::= "{" "}"|"{" BlockItem "}";
class Blockast : public BaseAST
{
public:
	unique_ptr<BaseAST> blockitem;
	string Dump() override
	{
		string temp;
		if (kind == 1)
			return temp;
		if (blockitem->kind != 1) // blockitem 不为空 为空返回空  空{}不计入block_num
		{
			//建立新的符号表
			fun_symtab.vec_symbolmap.push_back(symbolmap);
			int fun_block_num = fun_symtab.block_num;
			fun_symtab.block_cnt++;
			fun_symtab.block_num = fun_symtab.block_cnt;
			symbolmap.clear();
			
			temp = blockitem->Dump();
			
			symbolmap = fun_symtab.vec_symbolmap.back();
			fun_symtab.vec_symbolmap.pop_back();
			fun_symtab.block_num = fun_block_num;
			// if(ret_flag) ret_flag=false;
		}
		return temp;
	}
	
	int Calc() override
	{
		return 0;
	}
};
// BlockItem     ::=Decl|Stmt  |Decl BlockItem| Stmt BlockItem;
class BlockItemast : public BaseAST
{
public:
	unique_ptr<BaseAST> decl;
	unique_ptr<BaseAST> stmt;
	unique_ptr<BaseAST> blockitem;
	string Dump() override
	{
		string temp;
		if (kind == 1 || kind == 3) //为空返回值无用
		{
			decl->Dump();
		}
		else
		{
			stmt->Dump();
		}
		if (kind == 3 || kind == 4)
		{
			blockitem->Dump();
		}
		return temp;
	}
	
	int Calc() override
	{
		return 0;
	}
};

// ::= IDENT|IDENT ArrayExp;
class LValast : public BaseAST
{ //用符号表记录LVal和其对应的值
public:
	string ident;
	unique_ptr<BaseAST> arrayexp;
	string Dump() override //返回name
	{
		string temp;
		if (kind == 1)
		{
			temp = ident;
			vec_array_exp_len = 0;
		}
		else if (kind == 2)
		{
			temp = ident;
			arrayexp->Dump();
			vec_array_exp_len = arrayexp->Calc();
		}
		return temp;
	}
	int Calc() override //返回value  该函数只有const常量会调用需要
	{
		
		Symbol lval_sym = Symbol_find(ident);
		assert(lval_sym.kind == 0 || lval_sym.kind == 2);
		return lval_sym.val;
	}
};

// ArrayExp: '['Exp']'|'['Exp']' ArrayExp
class ArrayExpast : public BaseAST
{
public:
	// lval存在迭代 vec_array_exp 不好共用
	unique_ptr<BaseAST> exp;
	unique_ptr<BaseAST> arrayexp;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			vec_array_exp.push_back(exp->Dump());
		}
		else if (kind == 2)
		{
			vec_array_exp.push_back(exp->Dump());
			arrayexp->Dump();
		}
		return temp;
	}
	int Calc() override
	{ //需要计算数组迭代的长度
		int temp = 0;
		if (kind == 1)
		{
			// vec_array_exp.push_back(exp->Dump());
			temp = 1;
		}
		else if (kind == 2)
		{
			temp = arrayexp->Calc() + 1;
			// vec_array_exp.push_back(exp->Dump());
		}
		return temp;
	}
};

// PrimaryExp    ::= "(" Exp ")"  | Number| LVal;
class PrimaryExpast : public BaseAST // lval 分为常量和变量 常量直接换成number 变量需要load 数组需要load其需要的指针
{
public:
	unique_ptr<BaseAST> exp;
	int number;
	unique_ptr<BaseAST> lval;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = exp->Dump();
		}
		else if (kind == 2)
		{
			temp = to_string(number);
		}
		else if (kind == 3)
		{
			// Array_init();
			string ident = lval->Dump();
			Symbol lval_sym = Symbol_find(ident);
			if (lval_sym.type == "i32")
			{
				if (lval_sym.kind == 0 || lval_sym.kind == 2)
				{ // const常量
					temp = to_string(lval_sym.val);
				}
				else if (lval_sym.kind == 1 || lval_sym.kind == 3)
				{ // 变量
					temp = DumpLoad(ident, lval_sym.block_num);
				}
			}
			else //数组
			{
				
				if (lval_sym.val == vec_array_exp_len)
				{ //由于visit_array改变了vector就把判断放最前面了
					string get_array = Visit_array(ident, lval_sym.block_num, lval_sym.type);
					temp = "%" + to_string(++BaseAST::Count_Order);
					fprintf(IR, "  %s = load %s\n\n", temp.c_str(), get_array.c_str());
				}
				else
				{ //如果不等于代表此时应该参数数组转指针，指针转指针
					string get_array = Visit_array(ident, lval_sym.block_num, lval_sym.type);
					//Visit_array返回的是个数组
					if (get_array.empty())
					{
						if (lval_sym.block_num != -1)
						{
							get_array = "@" + ident + "_" + to_string(lval_sym.block_num);
						}
						else
							get_array = "@" + ident;
					}
					if (lval_sym.type[0] == '*' && vec_array_exp_len == 0)//仍然是指针
					{
						temp = get_array;
					} 
					else //数组 转化为指向自己第一个元素的指针
					{
						temp = "%" + to_string(++BaseAST::Count_Order);
						fprintf(IR, "  %s = getelemptr %s, 0\n\n", temp.c_str(), get_array.c_str());
					}
				}
			}
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = exp->Calc();
		}
		else if (kind == 2)
		{
			temp = number;
		}
		else if (kind == 3)
		{
			temp = lval->Calc();
		}
		return temp;
	}
};

// FuncDef     ::= Void IDENT "(" FuncArgs ")" Block|Btpye IDENT "(" FuncArgs")" Block ;
class FuncDefast : public BaseAST
{
public:
	unique_ptr<BaseAST> btype;
	string ident;
	unique_ptr<BaseAST> funcargs;
	unique_ptr<BaseAST> block;
	string Dump() override
	{
		string temp;
		Fun_init();
		fprintf(IR, "fun @%s(", ident.c_str());
		funcargs->Dump();
		if (kind == 1)
		{ // void 类型单独输出
			fprintf(IR, ") {\n%%entry_%d:\n",fun_num);
			fun_symtab.type = 1;
		}
		else if (kind == 2)
		{ //): %s{\n%%entry:\n
			fprintf(IR, "):%s {\n%%entry_%d:\n", btype->Dump().c_str(),fun_num);
			fprintf(IR, "  %%ret = alloc %s\n", btype->Dump().c_str());
			fun_symtab.type = 2;
		}
		all_fun_symtab.insert({ident, fun_symtab.type});
		funcargs->Dump();
		fprintf(IR, "  jump %%begin_%d\n\n%%begin_%d:\n",fun_num,fun_num);
		block->Dump();
		fprintf(IR,"  jump %%end_%d\n\n",fun_num);
		fprintf(IR, "%%end_%d:\n",fun_num);
		if (kind == 1)
		{
			fprintf(IR, "  ret\n");
		}
		else if (kind == 2)
		{
			temp = "%" + to_string(++Count_Order);
			fprintf(IR, "  %s = load %%ret\n", temp.c_str());
			fprintf(IR, "  ret %s\n",temp.c_str());
		}
		// if (!ret_flag)
		fprintf(IR, "}\n\n");
		return temp;
	}
	
	int Calc() override
	{
		return 0;
	}
};

// FuncArgs :|FuncFParams
class FuncArgsast : public BaseAST
{
public:
	unique_ptr<BaseAST> funcfparams;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
		}
		else if (kind == 2)
		{
			funcfparams->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// FuncFParams : FuncFParam| FuncFParam ',' FuncFParams;
class FuncFParamsast : public BaseAST
{
public:
	unique_ptr<BaseAST> funcfparam;
	unique_ptr<BaseAST> funcfparams;
	int Dump_cnt = 1;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			funcfparam->Dump();
		}
		else
		{
			funcfparam->Dump();
			if (Dump_cnt == 1)
			{
				fprintf(IR, " , ");
				Dump_cnt = 2;
			}
			else if (Dump_cnt == 2)
			{
				Dump_cnt = 1;
			}
			funcfparams->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// FuncFParam  : BType IDENT |BType IDENT []| BType IDENT [] ArrayDef;
class FuncFParamast : public BaseAST
{
public:
	unique_ptr<BaseAST> btype;
	string ident;
	unique_ptr<BaseAST> arraydef;
	string sym_cnt;
	string type;
	int Dump_cnt = 1; //函数参数要分两部分输出
	string Dump() override
	{
		string temp;
		Array_init();
		if (Dump_cnt == 1)
		{
			sym_cnt = "%" + to_string(++Count_Order);
			if (kind == 1)
			{
				type = btype->Dump();
			}
			else if (kind == 2)
			{
				type = "*" + btype->Dump();
			}
			else if (kind == 3)
			{
				type = "*" + arraydef->Dump();
			}
			fprintf(IR, "%s: %s", sym_cnt.c_str(), type.c_str());
			symbolmap.insert({ident, {1, int(vec_array_constexp.size()) + 1, type, fun_symtab.block_num}});
			Dump_cnt = 2;
		}
		else if (Dump_cnt == 2)
		{
			DumpAlloc(ident, fun_symtab.block_num, type);
			DumpStore(sym_cnt, ident, fun_symtab.block_num);
			Dump_cnt = 1;
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

// stmt : Exma | UExma
class Stmtast : public BaseAST
{
public:
	unique_ptr<BaseAST> exma;
	unique_ptr<BaseAST> uexma;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			exma->Dump();
		}
		else if (kind == 2)
		{
			uexma->Dump();
		}
		return temp;
	}
	
	int Calc() override
	{
		return 0;
	}
};

// UExma ->WHILE '(' Exp ')' UExma| IF '(' Exp ')' Stmt | IF '(' Exp ')' Exma ELSE UExma |
class UExmaast : public BaseAST
{
public:
	unique_ptr<BaseAST> stmt;
	unique_ptr<BaseAST> exp;
	unique_ptr<BaseAST> uexma;
	unique_ptr<BaseAST> exma;
	// int con_num; //记录此时if while的语句的次序
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{ // IF '(' Exp ')' Stmt    base_cnt //基本块划分IF WHILE 命令数
			While_cnt++;
			DumpWhile(exp, uexma, While_cnt);
		}
		else if (kind == 2)
		{
			IF_cnt++;
			DumpIf(exp, stmt, IF_cnt);
		}
		else if (kind == 3)
		{
			IF_cnt++;
			DumpIfElse(exp, exma, uexma, IF_cnt);
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

/*Exma          ::= return';'|"return" Exp ";"|LVal "=" Exp ";"| ';'|Exp ";"|Block
 | IF '(' Exp ')' Exma ELSE Exma   | WHILE '(' Exp ')' Exma | BREAK ';' | CONTINUE ';'*/
class Exmaast : public BaseAST
{
public:
	unique_ptr<BaseAST> exp;
	unique_ptr<BaseAST> lval;
	unique_ptr<BaseAST> block; //符号表迭代一层
	unique_ptr<BaseAST> exma_if;
	unique_ptr<BaseAST> exma_else;
	unique_ptr<BaseAST> exma_while;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{ // return';'
			
			fprintf(IR, "  jump %%end_%d\n\n",fun_num);
			fprintf(IR, "%%ret_%d:\n", ++ret_flag);
		}
		if (kind == 2) //"return" Exp ";"
		{
			fprintf(IR, "  store %s, %%ret\n\n", exp->Dump().c_str());
			fprintf(IR, "  jump %%end_%d\n\n",fun_num);
			fprintf(IR, "%%ret_%d:\n", ++ret_flag);
		}
		else if (kind == 3) // LVal "=" Exp ";"
		{
			// lval 在stmt需要 store ,lval 在exp中需要load
			Array_init();
			string temp_exp=exp->Dump();
			string ident = lval->Dump();
			Symbol lval_sym = Symbol_find(ident);
			if (lval_sym.type == "i32")
			{
				DumpStore(temp_exp, ident, lval_sym.block_num);
			}
			else
			{
				string temp_visit_aray= Visit_array(ident, lval_sym.block_num, lval_sym.type);
				fprintf(IR, "  store %s, %s\n", temp_exp.c_str(), temp_visit_aray.c_str());
			}
		}
		else if (kind == 4) //';'
		{
			// 空
		}
		else if (kind == 5) // exp ';'
		{
			exp->Dump();
		}
		else if (kind == 6) // Block
		{                   //符号表加深一层
			temp = block->Dump();
		}
		else if (kind == 7) // IF '(' Exp ')' Exma ELSE Exma
		{
			IF_cnt++;
			DumpIfElse(exp, exma_if, exma_else, IF_cnt);
		}
		else if (kind == 8) // WHILE '(' Exp ')' Exma
		{
			While_cnt++;
			DumpWhile(exp, exma_while, While_cnt);
		}
		else if (kind == 9) // BREAK ';'
		{
			Break_cnt++;
			int temp_while_cnt = vec_while.back();
			fprintf(IR, "  jump %%while_end_%d\n\n", temp_while_cnt);
			fprintf(IR, "%%while_body_%d_break_%d:\n", temp_while_cnt, Break_cnt);
		}
		else if (kind == 10) // CONTINUE ';'
		{
			Continue_cnt++;
			int temp_while_cnt = vec_while.back();
			fprintf(IR, "  jump %%while_entry_%d\n\n", temp_while_cnt);
			fprintf(IR, "%%while_body_%d_continue_%d:\n", temp_while_cnt, Continue_cnt);
		}
		return temp;
	}
	
	int Calc() override
	{
		return 0;
	}
};

class Expast : public BaseAST // Exp         ::= LOrExp;
{
public:
	unique_ptr<BaseAST> lorExp;
	
	string Dump() override
	{
		string temp = lorExp->Dump();
		return temp;
	}
	int Calc() override
	{
		return lorExp->Calc();
	}
};

// UnaryExp    : PrmaryExp|UnaryOp UnaryExp | IDENT '(' ')'|IDENT '(' FuncRParams')';
class UnaryExpast : public BaseAST
{
public:
	unique_ptr<BaseAST> primary;
	unique_ptr<BaseAST> unaryop;
	unique_ptr<BaseAST> unaryexp;
	string ident;
	unique_ptr<BaseAST> funcrparams;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = primary->Dump();
		}
		else if (kind == 2)
		{
			if (unaryop->kind == 1)
			{ //正,不需要处理
				temp = unaryexp->Dump();
			}
			else
			{
				temp = DumpUnaryOp(unaryexp->Dump(), unaryop->Dump());
			}
		}
		else if (kind == 3)
		{
			temp = DumpCall(ident, "");
			// fprintf(IR,"%%%d call @%s()",ident)
		}
		else if (kind == 4)
		{
			temp = DumpCall(ident, funcrparams->Dump());
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = primary->Calc();
		}
		else if (kind == 2)
		{
			temp = unaryexp->Calc();
			if (unaryop->kind == 1)
			{
				// + 忽略
			}
			else if (unaryop->kind == 2)
			{
				temp = -temp;
			}
			else if (unaryop->kind == 3)
			{
				temp = (temp == 0 ? 0 : 1);
			}
		}
		return temp;
	}
};

// FuncRParams:Exp|Exp ',' FuncRParams
class FuncRParamsast : public BaseAST
{
public:
	unique_ptr<BaseAST> funcrparams;
	unique_ptr<BaseAST> exp;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = exp->Dump();
		}
		else if (kind == 2)
		{
			temp = exp->Dump() + "," + funcrparams->Dump();
		}
		return temp;
	}
	int Calc() override
	{
		return 0;
	}
};

class MulExpast : public BaseAST // UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
{
public:
	unique_ptr<BaseAST> unaryExp;
	unique_ptr<BaseAST> mulop;
	unique_ptr<BaseAST> mulExp;
	
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = unaryExp->Dump();
		}
		else if (kind == 2)
		{
			//可再分temp为空
			temp = Dumpop(mulExp->Dump(), unaryExp->Dump(), mulop->Dump());
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = unaryExp->Calc();
		}
		else if (kind == 2)
		{
			
			int temp2 = mulExp->Calc();
			temp = unaryExp->Calc();
			if (mulop->kind == 1)
			{
				temp = temp2 * temp;
			}
			else if (mulop->kind == 2)
			{
				temp = temp2 / temp;
			}
			else if (mulop->kind == 3)
			{
				temp = temp2 % temp;
			}
		}
		return temp;
	}
};

class AddExpast : public BaseAST // MulExp | AddExp ("+" | "-") MulExp
{
public:
	unique_ptr<BaseAST> mulExp;
	unique_ptr<BaseAST> addOp;
	unique_ptr<BaseAST> addExp;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = mulExp->Dump();
		}
		else if (kind == 2)
		{
			temp = Dumpop(addExp->Dump(), mulExp->Dump(), addOp->Dump());
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = mulExp->Calc();
		}
		else if (kind == 2)
		{
			
			int temp2 = addExp->Calc();
			temp = mulExp->Calc();
			if (addOp->kind == 1)
			{
				temp = temp2 + temp;
			}
			else if (addOp->kind == 2)
			{
				temp = temp2 - temp;
			}
		}
		return temp;
	}
};

// RelExp      ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
class RelExpast : public BaseAST
{
public:
	unique_ptr<BaseAST> addexp;
	unique_ptr<BaseAST> relexp;
	unique_ptr<BaseAST> relop;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = addexp->Dump();
		}
		else if (kind == 2)
		{
			temp = Dumpop(relexp->Dump(), addexp->Dump(), relop->Dump());
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = addexp->Calc();
		}
		else if (kind == 2)
		{
			int temp_rel = relexp->Calc();
			temp = addexp->Calc();
			if (relop->kind == 1)
			{
				temp = (temp_rel < temp);
			}
			else if (relop->kind == 2)
			{
				temp = (temp_rel > temp);
			}
			else if (relop->kind == 3)
			{
				temp = (temp_rel <= temp);
			}
			else if (relop->kind == 4)
			{
				temp = (temp_rel >= temp);
			}
		}
		return temp;
	}
};

// EqExp       ::= RelExp | EqExp ("==" | "!=") RelExp;
class EqExpast : public BaseAST
{
public:
	unique_ptr<BaseAST> relexp;
	unique_ptr<BaseAST> eqexp;
	unique_ptr<BaseAST> eqop;
	
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = relexp->Dump();
		}
		else if (kind == 2)
		{
			temp = Dumpop(eqexp->Dump(), relexp->Dump(), eqop->Dump());
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = relexp->Calc();
		}
		else if (kind == 2)
		{
			if (eqop->kind == 1)
				temp = (eqexp->Calc() == relexp->Calc());
			else if (eqop->kind == 2)
				temp = (eqexp->Calc() != relexp->Calc());
		}
		return temp;
	}
};

// LAndExp     ::= EqExp | LAndExp "&&" EqExp;
class LAndExpast : public BaseAST
{
public:
	unique_ptr<BaseAST> eqexp;
	unique_ptr<BaseAST> op;
	unique_ptr<BaseAST> landexp;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = eqexp->Dump();
		}
		else if (kind == 2)
		{
			// 短路求值
			result_cnt++;
			int con_num = result_cnt;
			fprintf(IR, "  %%result_%d = alloc i32\n", con_num);
			fprintf(IR, "  store 0,%%result_%d\n", con_num);
			temp = DumpUnaryOp(landexp->Dump(), "ne");
			fprintf(IR, "  br %s, %%land_r%d, %%land_l%d\n\n", temp.c_str(), con_num, con_num);
			fprintf(IR, "%%land_r%d:\n", con_num);
			temp = DumpUnaryOp(eqexp->Dump(), "ne");
			fprintf(IR, " store %s,%%result_%d\n", temp.c_str(), con_num);
			fprintf(IR, "  jump %%land_l%d\n\n", con_num);
			
			fprintf(IR, "%%land_l%d:\n", con_num);
			Count_Order++;
			temp = "%" + to_string(Count_Order);
			fprintf(IR, "  %s= load %%result_%d\n", temp.c_str(), con_num);
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = eqexp->Calc();
		}
		else if (kind == 2)
		{
			temp = landexp->Calc() && eqexp->Calc();
		}
		return temp;
	}
};

// LOrExp      ::= LAndExp | LOrExp "||" LAndExp;
class LOrExpast : public BaseAST
{
public:
	unique_ptr<BaseAST> landexp;
	unique_ptr<BaseAST> op;
	unique_ptr<BaseAST> lorexp;
	string Dump() override
	{
		string temp;
		if (kind == 1)
		{
			temp = landexp->Dump();
		}
		else if (kind == 2)
		{
			//短路求值
			result_cnt++;
			int con_num = result_cnt;
			fprintf(IR, "  %%result_%d = alloc i32\n", con_num);
			fprintf(IR, "  store 1,%%result_%d\n", con_num);
			temp = DumpUnaryOp(lorexp->Dump(), "ge");
			fprintf(IR, "  br %s, %%lor_r%d, %%lor_l%d\n\n", temp.c_str(), con_num, con_num);
			fprintf(IR, "%%lor_r%d:\n", con_num);
			temp = DumpUnaryOp(landexp->Dump(), "ne");
			fprintf(IR, " store %s,%%result_%d\n", temp.c_str(), con_num);
			fprintf(IR, "  jump %%lor_l%d\n\n", con_num);
			
			fprintf(IR, "%%lor_l%d:\n", con_num);
			Count_Order++;
			temp = "%" + to_string(Count_Order);
			fprintf(IR, "  %s= load %%result_%d\n", temp.c_str(), con_num);
		}
		return temp;
	}
	
	int Calc() override
	{
		int temp = 0;
		if (kind == 1)
		{
			temp = landexp->Calc();
		}
		else if (kind == 2)
		{
			temp = lorexp->Calc() || landexp->Calc();
		}
		return temp;
	}
};

class Opast : public BaseAST
{
public:
	string str;
	string Dump() override
	{
		return str;
	}
	int Calc() override
	{
		return 0;
	}
};

#endif
