#!/bin/sh
set -e
# API on 8000, Web on 8001. Run both as children so neither gets SIGHUP from exec.
gunicorn -w 1 -b 0.0.0.0:8000 --access-logfile - --error-logfile - api:api_app &
GAPI_PID=$!
gunicorn -w 1 -b 0.0.0.0:8001 --access-logfile - --error-logfile - app:web_app &
GWEB_PID=$!

# Forward SIGTERM so docker stop cleans up both
trap 'kill $GAPI_PID $GWEB_PID 2>/dev/null; exit 0' TERM INT

wait
