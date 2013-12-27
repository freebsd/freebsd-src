public class Func_abc {
    public static void func_c() {
        System.out.println("Function C");
        try {
            Thread.currentThread().sleep(1000);
        } catch (Exception e) { }
    }
    public static void func_b() {
        System.out.println("Function B");
        try {
            Thread.currentThread().sleep(1000);
        } catch (Exception e) { }
        func_c();    
    }
    public static void func_a() {
        System.out.println("Function A");
        try {
            Thread.currentThread().sleep(1000);
        } catch (Exception e) { }
        func_b();
    }

    public static void main(String[] args) {
        func_a();
    }
}
