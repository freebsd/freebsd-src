#define _T(type) typeof(type)
#define pointer_to(type) _T(_T(type)*)
#define member_of(cls,type) _T(_T(type) cls::)
#define function(res, args) _T(_T(res) args)

#define _xq_yq(x,y) x ## _ ## y
#define _x_y(x,y) _xq_yq(x,y) 
#define _gensym(stem) _x_y(stem, __LINE__)
