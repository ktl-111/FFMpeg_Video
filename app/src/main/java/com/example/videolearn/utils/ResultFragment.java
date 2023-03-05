package com.example.videolearn.utils;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Created by steam_l on 2018/9/14.
 * Desprition :
 */

public class ResultFragment extends Fragment {
    private Map<Integer, ResultUtils.RequestCallBack> mRequestCallBackMap = new HashMap<>();
    private Map<Integer, ResultUtils.PermissionsCallBack> mPermissionsCallBack = new HashMap<>();
    private Map<Integer, ResultUtils.SinglePermissionsCallBack> mSinglePermissionsCallBack = new HashMap<>();

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRetainInstance(true);
    }

    public void startActivityForResult(Intent intent, int requestCode, ResultUtils.RequestCallBack callBack) {
        mRequestCallBackMap.put(requestCode, callBack);
        startActivityForResult(intent, requestCode);
    }

    public void requestPermissions(@NonNull String[] permissions, int requestCode, ResultUtils.PermissionsCallBack callBack) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            boolean tag = true;
            for (int i = 0; i < permissions.length; i++) {
                if (ContextCompat.checkSelfPermission(getContext(), permissions[i]) == PackageManager.PERMISSION_GRANTED) {
                    tag &= true;
                } else {
                    tag &= false;
                }
            }
            if (!tag) {
                mPermissionsCallBack.put(requestCode, callBack);
                requestPermissions(permissions, requestCode);
                return;
            }
        }
        List<String> strings = Arrays.asList(permissions);
        callBack.grantedList(strings);
    }

    public void requestSinglePermissions(@NonNull String permissions, int requestCode, ResultUtils.SinglePermissionsCallBack callBack) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (ContextCompat.checkSelfPermission(getContext(), permissions) != PackageManager.PERMISSION_GRANTED) {
                mSinglePermissionsCallBack.put(requestCode, callBack);
                requestPermissions(new String[]{permissions}, requestCode);
                return;
            }
        }
        callBack.isGranted(true);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (grantResults.length > 0) {
            boolean granted = true;
            List<String> deniedList = new ArrayList<>();
            List<String> grantedList = new ArrayList<>();
            for (int i = 0; i < grantResults.length; i++) {
                int grantResult = grantResults[i];
                if (grantResult == PackageManager.PERMISSION_DENIED) {//拒绝
                    deniedList.add(permissions[i]);
                    granted &= false;
                } else {
                    grantedList.add(permissions[i]);
                    granted &= true;
                }
            }
            ResultUtils.PermissionsCallBack callBack = mPermissionsCallBack.remove(requestCode);
            ResultUtils.SinglePermissionsCallBack singlePermissionsCallBack = mSinglePermissionsCallBack.remove(requestCode);
            if (singlePermissionsCallBack != null) {
                singlePermissionsCallBack.isGranted(granted);
            }
            if (callBack != null) {
                //                if (granted) {
                //                    callBack.granted();
                //                }
                callBack.deniedList(deniedList);
                callBack.grantedList(grantedList);
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        ResultUtils.RequestCallBack callBack = mRequestCallBackMap.remove(requestCode);
        if (callBack != null) {
            callBack.onActivityResult(requestCode, resultCode, data);
        }
    }
}
