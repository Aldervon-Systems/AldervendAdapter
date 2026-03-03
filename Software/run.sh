#!/bin/sh
set -e
# API on 8000, Web on 8001
gunicorn -w 1 -b 0.0.0.0:8000 --access-logfile - --error-logfile - api:api_app &
exec gunicorn -w 1 -b 0.0.0.0:8001 --access-logfile - --error-logfile - app:web_app
