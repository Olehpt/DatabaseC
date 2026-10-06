"""Run against a newly started server in an isolated directory: python tests/api_test.py <exe>."""
import concurrent.futures
import json
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

base = "http://127.0.0.1:18080"

def request(path, method="GET", body=None, expected=200, binary=False):
    data = body if binary else (json.dumps(body).encode() if body is not None else None)
    req = urllib.request.Request(base + path, data=data, method=method,
                                 headers={"Content-Type": "application/octet-stream" if binary else "application/json"})
    try:
        response = urllib.request.urlopen(req, timeout=10)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        content = response.read()
        assert response.code == expected, (path, method, response.code, expected, content)
        return content if binary else json.loads(content)

def exercise():
    assert request("/databases")["databases"] == []
    request("/databases/demo", "POST", expected=201)
    request("/databases/demo", "POST", expected=409)
    request("/databases/CON", "POST", expected=400)
    request("/databases/demo/tables/items", "POST", expected=201)
    table = "/databases/demo/tables/items"
    for name, kind in [("id", "integer"), ("price", "real"), ("code", "char"), ("name", "string")]:
        request(table + "/columns", "POST", {"name": name, "type": kind}, 201)
    request(table + "/columns", "POST", {"name": "id", "type": "integer"}, 409)
    assert request(table + "/rows")["rows"] == []
    values = [-2147483648, 2.5, "X", "Привет\u0000мир"]
    request(table + "/rows", "POST", {"values": values}, 201)
    assert request(table + "/rows/0")["values"] == values
    for bad in [[2147483648, 2.5, "X", "a"], [1.0, 2, "X", "a"], [True, 2, "X", "a"],
                [1, 2, "Ж", "a"], [1, 2, "XX", "a"], [1], [None, 2, "X", "a"]]:
        request(table + "/rows", "POST", {"values": bad}, 400)
    request(table + "/rows/0", "PATCH", {"values": {"name": "updated", "price": 3.75}})
    request(table + "/rows/0", "PATCH", {"values": {"missing": 1}}, 400)
    request(table + "/rows/0", "PATCH", {"values": {"name": "would change", "id": "bad"}}, 400)
    assert request(table + "/rows/0")["values"][3] == "updated"
    request(table + "/columns", "POST", {"name": "note", "type": "string", "default": "filled"}, 201)
    assert request(table + "/rows/0")["values"][-1] == "filled"
    request(table + "/columns/note", "PUT", {"name": "comment"})
    request(table + "/columns/comment", "DELETE")
    request(table + "/rows/0", "PUT", {"values": [2147483647, -1.25, "\u0000", "replaced"]})
    request(table + "/rows/50", expected=404)
    request(table + "/rows/-1", expected=400)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(lambda i: request(table + "/rows", "POST", {"values": [i, i / 2, "A", str(i)]}, 201), range(20)))
    assert len(request(table + "/rows")["rows"]) == 21
    exported = request("/databases/demo/export", binary=True)
    request("/databases/copy/import", "POST", exported, 201, binary=True)
    assert request("/databases/copy/export", binary=True) == exported
    request("/databases/copy/import", "POST", exported, 409, binary=True)
    for bad in [b"", exported[:-1], exported + b"junk", b"DBC1" + struct.pack("<I", 9) + exported[8:]]:
        request("/databases/copy/import", "PUT", bad, 400, binary=True)
        assert request("/databases/copy/export", binary=True) == exported
    request("/databases/legacy/import", "POST", exported[8:], 201, binary=True)
    assert request("/databases/legacy/export", binary=True) == exported
    waves = "/databases/copy/tables/waves"
    request(waves, "POST", expected=201)
    request(waves + "/columns", "POST", {"name": "z", "type": "complex"}, 201)
    z = {"real": 1.5, "imag": -2.5}
    request(waves + "/rows", "POST", {"values": [z]}, 201)
    assert request(waves + "/rows/0")["values"] == [z]
    assert request(waves + "/columns")["columns"][0]["type"] == "complex"
    for bad in [3, "1+2i", [1, 2], None, {"real": 1}, {"imag": 2},
                {"real": True, "imag": 2}, {"real": 1, "imag": "2"},
                {"real": 1, "imag": 2, "extra": 3}, {"real": 1e309, "imag": 2}]:
        request(waves + "/rows", "POST", {"values": [bad]}, 400)
        request(waves + "/rows/0", "PATCH", {"values": {"z": bad}}, 400)
        assert request(waves + "/rows/0")["values"] == [z]
    z = {"real": -4, "imag": 3}
    request(waves + "/rows/0", "PUT", {"values": [z]})
    z = {"real": 0, "imag": -7.25}
    request(waves + "/rows/0", "PATCH", {"values": {"z": z}})
    request(waves + "/columns", "POST", {"name": "zero", "type": "complex"}, 201)
    request(waves + "/columns", "POST", {"name": "preset", "type": "complex", "default": {"real": 2, "imag": 3}}, 201)
    assert request(waves + "/rows/0")["values"] == [z, {"real": 0, "imag": 0}, {"real": 2, "imag": 3}]
    request(waves + "/columns", "POST", {"name": "bad", "type": "complex", "default": 0}, 400)
    complex_export = request("/databases/copy/export", binary=True)
    request("/databases/complex_copy/import", "POST", complex_export, 201, binary=True)
    assert request("/databases/complex_copy/export", binary=True) == complex_export
    request("/databases/complex_copy/import", "PUT", complex_export[:-8], 400, binary=True)
    assert request("/databases/complex_copy/export", binary=True) == complex_export
    request(waves + "/rows/0", "DELETE")
    assert request(waves + "/rows")["rows"] == []
    request(waves, "DELETE")
    assert request("/databases/copy/export", binary=True) == exported
    request(table + "/rows/0", "DELETE")
    request(table + "/rows", "DELETE")
    assert request(table + "/rows")["rows"] == []
    request(table + "/columns/id", "DELETE")
    request(table, "PUT", {"name": "renamed"})
    request(table, expected=404)
    request("/databases/demo/tables/renamed", "DELETE")
    assert request("/databases/demo/tables")["tables"] == []
    request("/databases/demo", "DELETE")
    return exported

def start(exe, directory):
    process = subprocess.Popen([str(exe)], cwd=directory, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    for _ in range(100):
        if process.poll() is not None:
            raise RuntimeError("Server exited during startup")
        try:
            with urllib.request.urlopen(base, timeout=0.2) as response:
                if response.status == 200:
                    return process
        except (OSError, urllib.error.URLError):
            time.sleep(0.05)
    process.terminate()
    process.wait()
    raise RuntimeError("Server did not start")

if __name__ == "__main__":
    exe = pathlib.Path(sys.argv[1]).resolve()
    # Do not accidentally exercise an already running instance.
    with socket.socket() as probe:
        assert probe.connect_ex(("127.0.0.1", 18080)) != 0, "Port 18080 is already in use"
    with tempfile.TemporaryDirectory(prefix="api-test-", dir=exe.parent) as directory:
        process = start(exe, directory)
        try:
            exported = exercise()
            # Force a write failure without changing the committed file.
            pathlib.Path(directory, "copy.bin.tmp").mkdir()
            request("/databases/copy/tables/new", "POST", expected=500)
            assert request("/databases/copy/export", binary=True) == exported
            request("/databases/copy/import", "PUT", b"invalid", 400, binary=True)
        finally:
            process.terminate()
            process.wait(timeout=10)
        process = start(exe, directory)
        try:
            assert request("/databases/copy/export", binary=True) == exported
            assert request("/databases/legacy/export", binary=True) == exported
            assert request("/databases/complex_copy/tables/waves/rows/0")["values"][0] == {"real": 0, "imag": -7.25}
        finally:
            process.terminate()
            process.wait(timeout=10)
    print("API tests passed (CRUD, types, concurrency, binary import/export, restart, failed save)")
