use axum::{
    routing::get,
    Json, Router,
    response::IntoResponse,
};
use serde::Serialize;

#[derive(Serialize)]
struct Message {
    message: String,
}

async fn get_json() -> impl IntoResponse {
    Json(Message {
        message: "Hello World".to_string(),
    })
}

async fn get_text() -> impl IntoResponse {
    "Hello World"
}

#[tokio::main]
async fn main() {
    let app = Router::new()
        .route("/json", get(get_json))
        .route("/text", get(get_text));

    let listener = tokio::net::TcpListener::bind("127.0.0.1:6000").await.unwrap();
    axum::serve(listener, app).await.unwrap();
}
