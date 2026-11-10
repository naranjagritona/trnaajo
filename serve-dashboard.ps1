$ErrorActionPreference = "Stop"
# Busca la carpeta del script; si se ejecuta desde la consola, usa la carpeta actual.
$scriptPath = $MyInvocation.MyCommand.Path
$scriptDirectory = $null
if ($scriptPath) {
    $scriptDirectory = Split-Path -Parent $scriptPath
}
if (-not $scriptDirectory) {
    $scriptDirectory = (Get-Location).Path
}
$scriptDirectory = [IO.Path]::GetFullPath($scriptDirectory)
$root = $scriptDirectory
$spiffsDirectory = Join-Path $scriptDirectory "spiffs"
# Usa los archivos que están junto al script o, si no están, los de la carpeta spiffs.
if (-not [IO.File]::Exists((Join-Path $root "index.html"))) {
    if ([IO.File]::Exists((Join-Path $spiffsDirectory "index.html"))) {
        $root = [IO.Path]::GetFullPath($spiffsDirectory)
    } else {
        throw "No se encontró index.html junto al script ni en la subcarpeta spiffs de '$scriptDirectory'."
    }
}
# Prepara la ruta para impedir que una petición acceda fuera de la carpeta del dashboard.
$rootPrefix = $root
if (-not $rootPrefix.EndsWith([IO.Path]::DirectorySeparatorChar)) {
    $rootPrefix += [IO.Path]::DirectorySeparatorChar
}
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:8000/")

# Inicia el servidor y atiende las solicitudes del navegador.
try {
    $listener.Start()
    Write-Host "Dashboard disponible en http://localhost:8000"
    Write-Host "Archivos servidos desde: $root"
    Write-Host "Presiona Ctrl+C para detener el servidor."

    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $response = $context.Response

        # Solo entrega archivos solicitados con GET y ubicados en la carpeta permitida.
        try {
            if ($context.Request.HttpMethod -ne "GET") {
                $response.StatusCode = 405
                continue
            }

            $relativePath = [Uri]::UnescapeDataString($context.Request.Url.AbsolutePath.TrimStart('/'))
            if (-not $relativePath) {
                $relativePath = "index.html"
            }

            $filePath = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $relativePath))
            if (-not $filePath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -or
                -not [IO.File]::Exists($filePath)) {
                $response.StatusCode = 404
                continue
            }

            $response.ContentType = switch ([IO.Path]::GetExtension($filePath).ToLowerInvariant()) {
                ".html" { "text/html; charset=utf-8" }
                ".js"   { "application/javascript; charset=utf-8" }
                ".css"  { "text/css; charset=utf-8" }
                default { "application/octet-stream" }
            }
            $response.Headers.Add("Cache-Control", "no-store")

            $bytes = [IO.File]::ReadAllBytes($filePath)
            $response.ContentLength64 = $bytes.Length
            $response.OutputStream.Write($bytes, 0, $bytes.Length)
        }
        finally {
            # Cierra la respuesta incluso si hubo un problema al leer el archivo.
            $response.Close()
        }
    }
}
finally {
    # Detiene y libera el servidor al salir del script.
    if ($listener.IsListening) {
        $listener.Stop()
    }
    $listener.Close()
}
