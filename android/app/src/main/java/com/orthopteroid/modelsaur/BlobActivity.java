package com.orthopteroid.modelsaur;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

public class BlobActivity extends NativeActivity {

    private static final int PERM_WRITE_ACK = 999;

    public boolean mWriteGranted = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if(Build.VERSION.SDK_INT >= 23) {
            mWriteGranted = checkPerm(
                    Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    "Granting write access allows your designs to be saved.",
                    PERM_WRITE_ACK
            );
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        switch (requestCode) {
            case PERM_WRITE_ACK:
                mWriteGranted = grantResults[0] == PackageManager.PERMISSION_GRANTED; break;
            default:
                super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private boolean checkPerm(final String permission, final String reason, final int code) {
        if(checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED)
            return true;

        DialogInterface.OnClickListener listener = new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                requestPermissions( new String[] { permission }, code);
            }
        };

        new AlertDialog.Builder(this)
                .setMessage(reason)
                .setPositiveButton("OK", listener)
                .create()
                .show();

        return false; // will find out after callback
    }}
