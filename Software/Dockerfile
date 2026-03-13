FROM python:3.12-slim

WORKDIR /app

# Install deps (Pipfile + gunicorn)
COPY Pipfile Pipfile.lock ./
RUN pip install --no-cache-dir pipenv && pipenv install --system --deploy

# App code
COPY api.py app.py index.html style.css ./
COPY run.sh ./
RUN chmod +x run.sh

EXPOSE 8000 8001

# .local (SQLite + firmware) is created at runtime; mount a volume to persist
CMD ["./run.sh"]
