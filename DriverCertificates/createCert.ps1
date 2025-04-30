# createCert.ps1
$certName = "MyDriverCert"
$pfxPass = "mypassword"

Write-Host "[1] 자체 서명 인증서 생성 중..."

$cert = New-SelfSignedCertificate `
    -Type CodeSigning `
    -Subject "CN=$certName" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -KeyExportPolicy Exportable `
    -KeySpec Signature

Export-PfxCertificate `
    -Cert $cert `
    -FilePath ".\$certName.pfx" `
    -Password (ConvertTo-SecureString -String $pfxPass -Force -AsPlainText)

Export-Certificate `
    -Cert $cert `
    -FilePath ".\$certName.cer"

Write-Host "[✓] 생성 완료: $certName.pfx, $certName.cer"
