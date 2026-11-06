package flexzy;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;

public class FlexHandler implements InvocationHandler {
    public static native Object nativeOnTouch(Object[] args);

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) {
        return nativeOnTouch(args);
    }
}