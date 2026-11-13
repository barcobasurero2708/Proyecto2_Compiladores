FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PORT=10000 \
    ALLOW_CODE_EXECUTION=false \
    COMPILER_BRIDGE=/usr/local/bin/compiler-bridge

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ gcc python3 util-linux ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN set -eux; \
    SOURCES="$(find /app -maxdepth 1 -type f -name '*.cpp' ! -name 'main.cpp' | sort)"; \
    g++ -std=c++17 -O1 -I /app /app/frontend/bridge.cpp $SOURCES -o /usr/local/bin/compiler-bridge

RUN useradd --system --uid 10001 --home-dir /nonexistent --shell /usr/sbin/nologin appuser
USER appuser

EXPOSE 10000
CMD ["python3", "frontend/server.py"]
