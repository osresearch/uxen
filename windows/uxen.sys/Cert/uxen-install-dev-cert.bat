
certutil -addstore -user trustedpublisher dev-uxen.cer

rem CertMgr /add dev-xt2.cer /s /r localMachine root
rem CertMgr /add dev-xt2.cer /s /r localMachine trustedpublisher

