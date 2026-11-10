"""Bounded Linux subprocess output with a monotonic deadline."""
import codecs
import os
import selectors
import signal
import subprocess
import time

OUTPUT_LIMIT = 250_000


def run_command(command, timeout=120, on_output=None):
    """Run an argv; on_output must be fast and must not access Tk widgets.

    Merge stderr into stdout to preserve pipe ordering. Retain only the last
    OUTPUT_LIMIT characters. A deadline also covers descendants holding the pipe.
    Privileged descendants may outlive a timeout: never imply rollback.
    """
    if timeout <= 0:
        raise ValueError('Timeout must be positive')
    started = time.monotonic()
    output = ''
    truncated = False
    timed_out = False
    decoder = codecs.getincrementaldecoder('utf-8')(errors='replace')
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               stdin=subprocess.DEVNULL, shell=False, start_new_session=True)
    def emit(chunk):
        nonlocal output, truncated
        if not chunk:
            return
        output += chunk
        if len(output) > OUTPUT_LIMIT:
            truncated = True
            output = output[-OUTPUT_LIMIT:]
        if on_output is not None:
            on_output(chunk)

    try:
        with selectors.DefaultSelector() as selector:
            os.set_blocking(process.stdout.fileno(), False)
            selector.register(process.stdout, selectors.EVENT_READ)
            pipe_open = True
            while pipe_open or process.poll() is None:
                remaining = timeout - (time.monotonic() - started)
                if remaining <= 0:
                    timed_out = True
                    break
                ready = selector.select(min(remaining, 0.05)) if pipe_open else []
                if not pipe_open:
                    time.sleep(min(remaining, 0.01))
                for key, _ in ready:
                    data = os.read(key.fd, 4096)
                    if data:
                        emit(decoder.decode(data))
                    else:
                        selector.unregister(key.fileobj)
                        pipe_open = False
            emit(decoder.decode(b'', final=True))
    finally:
        # This group belongs only to this invocation; never kill by name.
        if timed_out or process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass
            if process.poll() is None:
                try:
                    process.kill()
                except (ProcessLookupError, PermissionError):
                    pass
        process.stdout.close()
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            pass  # Elevated children may deny signals; never block the UI forever.
    if timed_out:
        emit('\nTimed out. Check system state before retrying; changes may have partially completed. Privileged descendants may still be running.\n')
    return {'code':124 if timed_out else process.returncode, 'output':output,
            'truncated':truncated, 'seconds':round(time.monotonic()-started,2)}
