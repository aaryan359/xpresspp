from fastapi import FastAPI
from fastapi.responses import PlainTextResponse

app = FastAPI()

@app.get("/json")
async def get_json():
    return {"message": "Hello World"}

@app.get("/text", response_class=PlainTextResponse)
async def get_text():
    return "Hello World"
