package com.raylib.raymob;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;
import okio.ByteString;

public final class NetClient {
    private static final String SERVER_URL =
            "wss://jubilant-space-potato-wv4jqxqj4x4rh95v5-7777.app.github.dev/";
    private static final OkHttpClient CLIENT = new OkHttpClient();
    private static volatile WebSocket socket;

    private NetClient() {}

    public static void connect() {
        if (socket != null) return;
        Request request = new Request.Builder().url(SERVER_URL).build();
        socket = CLIENT.newWebSocket(request, new WebSocketListener() {
            @Override public void onOpen(WebSocket webSocket, Response response) {
                nativeStatus("ONLINE");
                webSocket.send("JOIN Survivor");
            }
            @Override public void onMessage(WebSocket webSocket, String text) {
                nativeMessage(text);
            }
            @Override public void onMessage(WebSocket webSocket, ByteString bytes) {
                nativeStatus("ОШИБКА: бинарное сообщение");
            }
            @Override public void onClosing(WebSocket webSocket, int code, String reason) {
                nativeStatus("ОТКЛЮЧЕНИЕ");
                webSocket.close(code, reason);
            }
            @Override public void onClosed(WebSocket webSocket, int code, String reason) {
                socket = null;
                nativeStatus("OFFLINE");
            }
            @Override public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                socket = null;
                nativeStatus("НЕТ СВЯЗИ");
            }
        });
    }

    public static void send(String message) {
        WebSocket current = socket;
        if (current != null) current.send(message);
    }

    public static native void nativeMessage(String message);
    public static native void nativeStatus(String status);
}
