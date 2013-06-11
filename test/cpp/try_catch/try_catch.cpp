#include <iostream>

void throwFun() throw(int) {
	throw 1;
}

int throwFun1() throw(int) {
	throw 1;
	return 1;
}

struct Obj {
	Obj() : a(10) {}
	int a;
};

int main() {

	try { throw 10; } catch(int e) { std::cout << "exception caught " << e << std::endl; }

	int x = 10;
	try { throw x; } catch(int e) { std::cout << "exception caught " << e << std::endl; }
	try{ throwFun(); } catch(int e) { std::cout << "exception caught " << e << std::endl; }
	try { x = throwFun1(); } catch(int e) { std::cout << "exception caught " << e << std::endl; }
	try{ throwFun(); } catch(...) { std::cout << "exception caught " << std::endl; }
	try{ x = throwFun1(); } catch(...) { std::cout << "exception caught " << std::endl; }
	
	{
		Obj a;
		try { throw a; } catch(Obj e) { std::cout << "exception caught " << e.a << std::endl; }
	}

	try { throw Obj(); } catch(Obj e) { std::cout << "exception caught " << e.a << std::endl; }

	try { Obj(); } catch(Obj& e) { std::cout << "exception caught " << e.a << std::endl; }

	return 0;
}

