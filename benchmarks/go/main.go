package main

import (
	"encoding/json"
	"net/http"
	"runtime"
)

type Message struct {
	Message string `json:"message"`
}

func main() {
	// Explicitly utilize all available CPU cores for the Go runtime
	runtime.GOMAXPROCS(runtime.NumCPU())

	http.HandleFunc("/json", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(Message{Message: "Hello World"})
	})

	http.HandleFunc("/text", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain")
		w.Write([]byte("Hello World"))
	})

	http.ListenAndServe(":5000", nil)
}
