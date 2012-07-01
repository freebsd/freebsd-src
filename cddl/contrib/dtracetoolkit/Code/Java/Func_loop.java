public class Func_loop {
    public static void func_c() {
        System.out.println("Function C");
        while (true) {
        }
    }
    public static void func_b() {
        System.out.println("Function B");
        func_c();    
    }
    public static void func_a() {
        System.out.println("Function A");
        func_b();
    }

    public static void main(String[] args) {
        func_a();
    }
}
