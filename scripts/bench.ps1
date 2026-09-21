param(
    [int]$Clients = 200,
    [int]$Workers = 16,
    [int]$Iterations = 20,
    [int]$Port = 9084
)

$ErrorActionPreference = "Stop"
$exe = Join-Path (Split-Path $PSScriptRoot -Parent) "build\server\RegistroServer.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Server binary not found - build first: .\scripts\run-vs.cmd cmake --workflow --preset ci"
    exit 1
}
if ($env:CMAKE_PREFIX_PATH) {
    $env:PATH = "$env:CMAKE_PREFIX_PATH\bin;$env:PATH"
}

$base = "http://127.0.0.1:$Port"
$server = Start-Process -FilePath $exe -ArgumentList "--port", "$Port", "--db", ":memory:",
    "--admin-user", "admin", "--admin-password", "secret123" -PassThru -WindowStyle Hidden

try {
    $up = $false
    for ($i = 0; $i -lt 50; $i++) {
        try {
            if ((Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1).StatusCode -eq 200) {
                $up = $true
                break
            }
        } catch { }
        Start-Sleep -Milliseconds 200
    }
    if (-not $up) { throw "server did not start on port $Port" }

    $login = Invoke-RestMethod -Method Post -ContentType "application/json" `
        -Body '{"username":"admin","password":"secret123"}' -Uri "$base/api/auth/login"
    $headers = @{ Authorization = "Bearer $($login.token)" }

    $seed = Measure-Command {
        1..$Clients | ForEach-Object -Parallel {
            $h = $using:headers
            $b = $using:base
            $body = @{ full_name = "Bench $_" } | ConvertTo-Json
            Invoke-RestMethod -Method Post -ContentType "application/json" -Headers $h -Body $body `
                -Uri "$b/api/clients" | Out-Null
        } -ThrottleLimit 16
    }

    $page = Invoke-RestMethod -Headers $headers -Uri "$base/api/clients?limit=1"
    if ($page.total -ne $Clients) {
        throw "seed mismatch: total=$($page.total) expected=$Clients"
    }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    1..$Workers | ForEach-Object -Parallel {
        $h = $using:headers
        $b = $using:base
        for ($i = 0; $i -lt $Iterations; $i++) {
            Invoke-RestMethod -Headers $h -Uri "$b/api/clients?limit=50" | Out-Null
        }
    } -ThrottleLimit $Workers
    $sw.Stop()

    $total = $Workers * $Iterations
    $secs = [math]::Max($sw.Elapsed.TotalSeconds, 0.001)
    $rps = [math]::Round($total / $secs, 1)
    $avg = [math]::Round($sw.Elapsed.TotalMilliseconds / $total, 2)
    Write-Host "seed: $Clients clients in $([math]::Round($seed.TotalSeconds, 2))s"
    Write-Host "bench: $total GETs ($Workers x $Iterations) in $([math]::Round($sw.Elapsed.TotalSeconds, 2))s -> $rps RPS, avg $avg ms/req"
    Write-Host "consistency: total=$($page.total)"
} finally {
    if ($server -and -not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}