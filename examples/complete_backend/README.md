# Xpress++ Structured MVC Backend

This example demonstrates a complete, production-grade Model-View-Controller (MVC) directory structure using **Xpress++**. The application features structured routing, controller actions, custom middleware, and asynchronous PostgreSQL database interaction using C++20 coroutines.

## Directory Structure

```text
complete_backend/
├── CMakeLists.txt              # CMake build definition
├── vcpkg.json                  # Optional package manager manifest
├── main.cpp                    # Clean entrypoint (imports src/exports.h)
└── src/
    ├── exports.h               # Central index exporting all modules in one place
    ├── config/                 # Database configurations (Header-Only)
    │   └── db.h
    ├── middleware/             # Middlewares (Header-Only)
    │   ├── logger.h            # Custom HTTP logger
    │   └── auth.h              # JWT Token validation
    ├── controllers/            # Controller layers (Header-Only)
    │   ├── auth_controller.h   # User signup & login actions
    │   └── user_controller.h   # Profile fetch & listing actions
    └── routes/                 # Routing layers (Header-Only)
        └── api_routes.h

```

---

## API Documentation

### Public Endpoints

#### 1. Public Home
*   **Method**: `GET`
*   **Path**: `/`
*   **Response**: `text/html` listing available API endpoints.

#### 2. User Registration (Signup)
*   **Method**: `POST`
*   **Path**: `/api/auth/signup`
*   **Request Body** (`application/json`):
    ```json
    {
      "username": "aaryan",
      "password": "securepassword"
    }
    ```
*   **Response** (`application/json`):
    *   `200 OK`: `{"success": true, "message": "User registered successfully"}`
    *   `400 Bad Request`: `{"error": "Registration failed (username may already be taken)"}`

#### 3. User Authentication (Login)
*   **Method**: `POST`
*   **Path**: `/api/auth/login`
*   **Request Body** (`application/json`):
    ```json
    {
      "username": "aaryan",
      "password": "securepassword"
    }
    ```
*   **Response** (`application/json`):
    *   `200 OK`: `{"success": true, "token": "<JWT_BEARER_TOKEN>"}`
    *   `401 Unauthorized`: `{"error": "Invalid username or password"}`

---

### Protected Endpoints (Requires `Authorization: Bearer <JWT_BEARER_TOKEN>`)

#### 4. List All Users
*   **Method**: `GET`
*   **Path**: `/api/users`
*   **Response** (`application/json`):
    ```json
    [
      {
        "id": 1,
        "username": "aaryan",
        "created_at": "2026-06-08 02:40:00"
      }
    ]
    ```

#### 5. Authenticated User Profile
*   **Method**: `GET`
*   **Path**: `/api/users/profile`
*   **Response** (`application/json`):
    ```json
    {
      "success": true,
      "profile": {
        "id": 1,
        "username": "aaryan",
        "created_at": "2026-06-08 02:40:00"
      }
    }
    ```

---

## Build and Run Instructions

### 1. Requirements
*   PostgreSQL running on `127.0.0.1:5450` (database `xpresspp`, username `postgres`, password `password`).
*   GCC or Clang with C++20 support.
*   CMake 3.16+.

### 2. Compilation
Compile the project using CMake:
```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

### 3. Execution
Run the compiled executable:
```bash
./xpresspp_complete_backend
```

The server will initialize the connection pool, run the table migrations, and listen on **`http://localhost:8082`**.
